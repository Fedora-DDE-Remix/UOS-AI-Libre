#include "appcontroller.h"
#include "backend/shortcuts/shortcutservice.h"
#include "dde/dde_service.h"
#include "quickaiwindow.h"
#include "singleinstance.h"
#include "waylanddrag.h"
#include "windowhost.h"
#include "iconprovider.h"
#include "rhibackend-selector.h"

#include <QDir>
#include <QFont>
#include <QFontDatabase>
#include <QApplication>
#include <QSessionManager>
#include <QIcon>
#include <QImage>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQmlContext>
#include <QMetaObject>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <memory>

int main(int argc, char *argv[]) {
    QApplication qtApp(argc, argv);
    QGuiApplication::setQuitOnLastWindowClosed(false);
    QGuiApplication::setApplicationName(QStringLiteral("UOS AI Libre"));
    QGuiApplication::setOrganizationName(QStringLiteral("UOS-AI-Libre"));
    QGuiApplication::setApplicationVersion(QStringLiteral("1.0.0"));
    QGuiApplication::setDesktopFileName(QStringLiteral("uos-ai-libre"));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/images/UosAiLibre.svg")));

    QQuickStyle::setStyle(QStringLiteral("Basic"));

    QFont font;
    const QFontDatabase database;
    const QStringList families = database.families();
    for (const QString &candidate :
         {QStringLiteral("Noto Sans CJK SC"), QStringLiteral("Source Han Sans SC"),
          QStringLiteral("Microsoft YaHei UI"), QStringLiteral("Noto Sans"),
          QStringLiteral("DejaVu Sans")}) {
        if (families.contains(candidate)) {
            font.setFamily(candidate);
            break;
        }
    }
    font.setPixelSize(14);
    QGuiApplication::setFont(font);

    const QStringList arguments = QCoreApplication::arguments();
    const bool service_mode =
        arguments.contains(QStringLiteral("--service"))
        || arguments.contains(QStringLiteral("--dde-service"));

    // The agent backend lives in this process. Closing the window only
    // destroys the QML window; a second launch asks the first instance to show
    // a fresh window backed by the still-running ChatModel.
    SingleInstance instance;
    if (!instance.isPrimary())
        return 0;

    // Keep the DDE/Treeland/X11 global shortcut policy in one service. The
    // DDE backend is idempotent and replaces the installer's helper entry.
    ShortcutService shortcuts;
    QString shortcut_error;
    QString shortcut_exec = QCoreApplication::applicationFilePath();
    shortcut_exec.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    const QString shortcut_command =
        QLatin1Char('"') + shortcut_exec + QStringLiteral("\" --chat");
    if (!shortcuts.registerShortcut(QStringLiteral("UOS AI Libre"),
                                    QStringLiteral("Meta+Space"),
                                    shortcut_command, &shortcut_error)) {
        qWarning().noquote() << "global shortcut:" << shortcut_error
                             << "backend:" << shortcuts.backendName();
    }

    // Pick the scene-graph backend once, before any QQuickWindow exists.
    QString rhiDetail;
    const QSGRendererInterface::GraphicsApi rhi = RhiBackendSelector::select(&rhiDetail);
    if (rhi != QSGRendererInterface::Unknown)
        QQuickWindow::setGraphicsApi(rhi);
    qInfo().noquote() << "RHI backend:" << rhiDetail;

    AppController app;
    // Declared before the QML engine so its destructor (which tears down the
    // Wayland proxies) runs after all QML objects are gone.
    WaylandDragTracker quick_ai_drag;

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("appicon"), new AppIconProvider);
    engine.rootContext()->setContextProperty(QStringLiteral("quickAiDrag"),
                                             &quick_ai_drag);
    engine.rootContext()->setContextProperty(QStringLiteral("app"), &app);
    engine.rootContext()->setContextProperty(QStringLiteral("appVersion"),
                                             qtApp.applicationVersion());

    WindowHost window_host(&engine, &app);
    engine.rootContext()->setContextProperty(QStringLiteral("windowHost"),
                                             &window_host);

    // The tray lives outside the main window so it can reopen the UI after the
    // QML window (and its QQuickWindow) has been destroyed.
    QQmlComponent shell_component(&engine);
    shell_component.loadFromModule(QStringLiteral("UosAiLibre"),
                                   QStringLiteral("Shell"));
    if (shell_component.isError()) {
        qWarning() << "failed to load Shell.qml:" << shell_component.errorString();
    } else if (QObject *shell = shell_component.create(engine.rootContext())) {
        shell->setParent(&engine);
    }

    QObject::connect(&instance, &SingleInstance::showRequested, &window_host,
                     &WindowHost::showWindow);
    QObject::connect(&qtApp, &QGuiApplication::commitDataRequest,
                     &window_host, [&app, &window_host](QSessionManager &) {
                         app.markSavingSession();
                         window_host.quitApp();
                     });
    QObject::connect(&qtApp, &QGuiApplication::saveStateRequest,
                     &window_host, [&app, &window_host](QSessionManager &) {
                         app.markSavingSession();
                         window_host.quitApp();
                     });

    DdeService dde;
    QuickAiWindowHost quick_ai(&engine, &app);
    QObject::connect(&dde, &DdeService::chatPageRequested, &window_host,
                     [&window_host](int) { window_host.showChatPage(); });
    QObject::connect(&dde, &DdeService::chatPageWithTokenRequested,
                     &window_host, [&window_host](int, const QString &) {
                         window_host.showChatPage();
                     });
    QObject::connect(&dde, &DdeService::chatImageRequested, &window_host,
                     [&window_host, &app](const QString &path) {
                         window_host.showChatPage();
                         app.queueAttachments({path});
                     });
    QObject::connect(&dde, &DdeService::quickImageRequested, &quick_ai,
                     [&quick_ai, &window_host, &app,
                      &quick_ai_drag](int mode, const QString &path,
                                      const QPoint &anchor) {
                         quick_ai_drag.end();
                         quick_ai.show(mode, path, anchor);
                         if (!quick_ai.visible()) {
                             // If the layer-shell surface could not be created,
                             // keep the feature usable in the main chat.
                             window_host.showChatPage();
                             app.startQuickImageAction(mode, path);
                             return;
                         }
                         if (!app.startQuickImageAction(mode, path))
                             quick_ai.setError(QStringLiteral(
                                 "无法启动 AI 快捷分析，请确认模型配置和当前会话状态。"));
                     });
    QObject::connect(&dde, &DdeService::promptRequested, &window_host,
                     [&window_host, &app](const QString &question,
                                          const QString &file,
                                          const QString &defaultPrompt) {
                         window_host.showChatPage();
                         app.startDocumentPrompt(question, file, defaultPrompt);
                     });
    QObject::connect(&dde, &DdeService::settingsRequested, &window_host,
                     [&window_host](bool) { window_host.showSettingsPage(0); });
    QObject::connect(&dde, &DdeService::translationRequested, &window_host,
                     [&window_host, &app]() {
                         window_host.showChatPage();
                         app.translateSelection();
                     });
    QObject::connect(&dde, &DdeService::screenshotRequested, &window_host,
                     [&window_host, &app]() {
                         window_host.showChatPage();
                         const QString path = app.takeScreenshot();
                         if (path.isEmpty())
                             app.showToast(QStringLiteral("截图失败"));
                         else
                             app.queueAttachments({path});
                     });
    QObject::connect(&dde, &DdeService::wordWizardRequested, &window_host,
                     [&window_host]() { window_host.showChatPage(); });

    const auto update_dde_capabilities = [&app, &dde]() {
        dde.setTranslationAvailable(app.ready()
                                    && !app.model().trimmed().isEmpty());
    };
    QObject::connect(&app, &AppController::readyChanged, &dde,
                     update_dde_capabilities);
    QObject::connect(&app, &AppController::modelChanged, &dde,
                     update_dde_capabilities);

    if (service_mode)
        app.initialize();

    QString dde_error;
    if (!dde.start(&dde_error)) {
        qWarning().noquote() << "DDE compatibility service unavailable:"
                             << dde_error;
        if (service_mode)
            return 0;
        // A running UOS AI instance may own the names. Retry in the background
        // so stopping/replacing it does not require restarting this GUI.
        auto *retry = new QTimer(&app);
        retry->setInterval(5000);
        QObject::connect(retry, &QTimer::timeout, &dde,
                         [&dde, &dde_error, retry]() {
                             if (dde.start(&dde_error)) {
                                 retry->stop();
                                 qInfo().noquote()
                                     << "DDE compatibility service registered";
                             }
                         });
        retry->start();
    }
    update_dde_capabilities();

    if (!service_mode) {
        window_host.showWindow();
        QTimer::singleShot(0, &app, &AppController::initialize);
    }

    QQuickWindow *window = window_host.window();
    QObject *root = window;

    const QString theme = qEnvironmentVariable("UOS_AI_LIBRE_THEME");
    if (theme == QStringLiteral("light"))
        app.setProperty("themeMode", 0);
    else if (theme == QStringLiteral("dark"))
        app.setProperty("themeMode", 1);

    const QString snapshot_path =
        qEnvironmentVariable("UOS_AI_LIBRE_SNAPSHOT");
    if (!service_mode && !snapshot_path.isEmpty()) {
        const QString page =
            qEnvironmentVariable("UOS_AI_LIBRE_SNAPSHOT_PAGE",
                                 QStringLiteral("welcome"));
        const QString session =
            qEnvironmentVariable("UOS_AI_LIBRE_SNAPSHOT_SESSION");
        QTimer::singleShot(250, root, [&app, root, page, session]() {
            app.initialize();
            if (!session.isEmpty())
                app.loadSession(session);
            if (page == QStringLiteral("settings")) {
                const int index =
                    qEnvironmentVariableIntValue("UOS_AI_LIBRE_SETTINGS_INDEX");
                if (index > 0)
                    QMetaObject::invokeMethod(root, "showSettings",
                                              Q_ARG(QVariant, QVariant(index)));
                else
                    root->setProperty("activePage", QStringLiteral("settings"));
            } else if (page == QStringLiteral("approval")) {
                QVariantMap request;
                request.insert(QStringLiteral("request_id"), QStringLiteral("demo-approval"));
                request.insert(QStringLiteral("tool_name"), QStringLiteral("WebFetch"));
                request.insert(QStringLiteral("target"), QStringLiteral("https://example.com/"));
                request.insert(QStringLiteral("risk"), QStringLiteral("MEDIUM"));
                request.insert(QStringLiteral("mode"), QStringLiteral("confirm"));
                request.insert(QStringLiteral("allow"), true);
                request.insert(QStringLiteral("message"),
                               QStringLiteral("AI 需要访问该网页以获取页面标题。"));
                request.insert(QStringLiteral("cwd"), QDir::currentPath());
                QVariantMap rule;
                rule.insert(QStringLiteral("id"), QStringLiteral("M-001"));
                rule.insert(QStringLiteral("name"), QStringLiteral("家目录保护"));
                rule.insert(QStringLiteral("pattern"), QStringLiteral("/home/**"));
                QVariantMap sandbox;
                sandbox.insert(QStringLiteral("enabled"), false);
                QVariantMap analysis;
                analysis.insert(QStringLiteral("matched_rule"), rule);
                analysis.insert(QStringLiteral("reasons"),
                                QVariantList{QStringLiteral("测试安全规则命中")});
                analysis.insert(QStringLiteral("suggested_alternatives"),
                                QVariantList{QStringLiteral("确认目标地址后再允许")});
                analysis.insert(QStringLiteral("sandbox"), sandbox);
                request.insert(QStringLiteral("analysis"), analysis);
                root->setProperty("activePage", QStringLiteral("chat"));
                QMetaObject::invokeMethod(root, "showDemoApproval",
                                          Q_ARG(QVariant, QVariant::fromValue(request)));
            } else if (page == QStringLiteral("interaction")) {
                root->setProperty("activePage", QStringLiteral("chat"));
                QTimer::singleShot(250, root, [root]() {
                    QMetaObject::invokeMethod(root, "showDemoInteraction");
                });
            } else if (page == QStringLiteral("menu")) {
                QMetaObject::invokeMethod(root, "openAppMenu");
            } else if (page == QStringLiteral("theme")) {
                QMetaObject::invokeMethod(root, "openAppMenu");
                QTimer::singleShot(200, root, [root]() {
                    QMetaObject::invokeMethod(root, "openThemeMenu");
                });
            } else if (page == QStringLiteral("context")) {
                root->setProperty("activePage", QStringLiteral("chat"));
                QTimer::singleShot(400, root, [root]() {
                    QMetaObject::invokeMethod(root, "openHistoryContext");
                });
            } else if (page == QStringLiteral("rename")) {
                root->setProperty("activePage", QStringLiteral("chat"));
                QTimer::singleShot(400, root, [root]() {
                    QMetaObject::invokeMethod(root, "startHistoryRename");
                });
            } else if (page == QStringLiteral("composer")) {
                root->setProperty("activePage", QStringLiteral("chat"));
                QTimer::singleShot(250, root, [root]() {
                    QMetaObject::invokeMethod(root, "showComposerMenu");
                });
            } else if (page == QStringLiteral("tasks")) {
                root->setProperty("activePage", QStringLiteral("tasks"));
            } else if (page == QStringLiteral("knowledge")) {
                root->setProperty("activePage", QStringLiteral("knowledge"));
            } else if (page == QStringLiteral("collapsed")) {
                root->setProperty("activePage", QStringLiteral("chat"));
                root->setProperty("sidebarCollapsed", true);
            } else {
                root->setProperty("activePage", QStringLiteral("chat"));
            }
        });
        const QString prompt =
            qEnvironmentVariable("UOS_AI_LIBRE_SNAPSHOT_PROMPT");
        auto snapshot_done = std::make_shared<bool>(false);
        auto grab = [window, snapshot_path, snapshot_done, &qtApp]() {
            if (*snapshot_done)
                return;
            *snapshot_done = true;
            const QImage image = window->grabWindow();
            if (!image.isNull())
                image.save(snapshot_path);
            QTimer::singleShot(150, &qtApp, []() { QCoreApplication::quit(); });
        };
        if (!prompt.isEmpty()) {
            QObject::connect(&app, &AppController::generatingChanged, root,
                             [&app, grab]() {
                                 if (!app.generating())
                                     QTimer::singleShot(450, grab);
                             });
            QTimer::singleShot(350, root, [&app, root, prompt, grab]() {
                root->setProperty("activePage", QStringLiteral("chat"));
                app.newConversation();
                if (!app.sendMessage(prompt, {}))
                    QTimer::singleShot(500, grab);
            });
            QTimer::singleShot(120000, root, grab);
        } else {
            QTimer::singleShot(1600, root, grab);
        }
    }

    return qtApp.exec();
}
