#include "appcontroller.h"
#include "singleinstance.h"
#include "windowhost.h"
#include "iconprovider.h"

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
    QGuiApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    QGuiApplication::setDesktopFileName(QStringLiteral("uos-ai-libre"));
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/images/logo.png")));

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

    // The agent backend lives in this process. Closing the window only
    // destroys the QML window; a second launch asks the first instance to show
    // a fresh window backed by the still-running ChatModel.
    SingleInstance instance;
    if (!instance.isPrimary())
        return 0;

    AppController app;

    QQmlApplicationEngine engine;
    engine.addImageProvider(QStringLiteral("appicon"), new AppIconProvider);
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

    window_host.showWindow();
    QQuickWindow *window = window_host.window();
    if (!window)
        return 1;
    QObject *root = window;

    QTimer::singleShot(0, &app, &AppController::initialize);

    const QString theme = qEnvironmentVariable("UOS_AI_LIBRE_THEME");
    if (theme == QStringLiteral("light"))
        app.setProperty("themeMode", 0);
    else if (theme == QStringLiteral("dark"))
        app.setProperty("themeMode", 1);

    const QString snapshot_path =
        qEnvironmentVariable("UOS_AI_LIBRE_SNAPSHOT");
    if (!snapshot_path.isEmpty()) {
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
                root->setProperty("activePage", QStringLiteral("chat"));
                QMetaObject::invokeMethod(root, "showDemoApproval",
                                          Q_ARG(QVariant, QVariant::fromValue(request)));
            } else if (page == QStringLiteral("menu")) {
                QMetaObject::invokeMethod(root, "openAppMenu");
            } else if (page == QStringLiteral("theme")) {
                QMetaObject::invokeMethod(root, "openAppMenu");
                QTimer::singleShot(200, root, [root]() {
                    QMetaObject::invokeMethod(root, "openThemeMenu");
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
