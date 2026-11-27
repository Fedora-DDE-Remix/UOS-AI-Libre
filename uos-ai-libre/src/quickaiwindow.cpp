#include "quickaiwindow.h"

#include "appcontroller.h"

#include <QEvent>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QDebug>

QuickAiWindowHost::QuickAiWindowHost(QQmlEngine *engine, AppController *app,
                                     QObject *parent)
    : QObject(parent), m_engine(engine), m_app(app) {}

QuickAiWindowHost::~QuickAiWindowHost() { close(); }

bool QuickAiWindowHost::visible() const {
    return m_window && m_window->isVisible();
}

bool QuickAiWindowHost::eventFilter(QObject *watched, QEvent *event) {
    // On Wayland the compositor may move pointer focus while a drag is in
    // progress. The layer-shell client then never sees the button release.
    // A window-level Leave is the reliable capture-loss signal; bump an epoch
    // consumed by QuickAiPanel so it can stop the relative-pointer drag.
    if (m_window && watched == m_window && event->type() == QEvent::Leave) {
        const int epoch = m_window->property("dragCancelEpoch").toInt();
        m_window->setProperty("dragCancelEpoch", epoch + 1);
    }
    return QObject::eventFilter(watched, event);
}

QQmlComponent *QuickAiWindowHost::loadComponent(bool layerShell) {
    auto *component = new QQmlComponent(m_engine, this);
    component->loadFromModule(QStringLiteral("UosAiLibre"),
                              layerShell ? QStringLiteral("QuickAiWindow")
                                         : QStringLiteral("QuickAiFallbackWindow"));
    if (component->isError()) {
        qWarning().noquote()
            << "failed to load quick AI surface:"
            << component->errorString();
        delete component;
        return nullptr;
    }
    return component;
}

bool QuickAiWindowHost::create(bool preferLayerShell) {
    QQmlComponent *component = nullptr;
    if (preferLayerShell) {
        if (!m_layer_component)
            m_layer_component = loadComponent(true);
        component = m_layer_component;
    }
    if (!component) {
        if (!m_fallback_component)
            m_fallback_component = loadComponent(false);
        component = m_fallback_component;
    }
    if (!component)
        return false;

    QObject *object = component->create(m_engine->rootContext());
    auto *window = qobject_cast<QQuickWindow *>(object);
    if (!window) {
        if (object)
            object->deleteLater();
        return false;
    }

    m_window = window;
    window->installEventFilter(this);
    connect(window, &QObject::destroyed, this, [this, window]() {
        if (m_window == window)
            m_window = nullptr;
    });
    connect(window, &QQuickWindow::closing, this,
            [this](QQuickCloseEvent *) {
                if (!m_window)
                    return;
                QQuickWindow *closingWindow = m_window;
                m_window = nullptr;
                closingWindow->deleteLater();
            });
    return true;
}

void QuickAiWindowHost::show(int mode, const QString &imagePath,
                             const QPoint &anchorPoint) {
    close();
    const QString platform = QGuiApplication::platformName().toLower();
    const bool preferLayerShell =
        platform.contains(QStringLiteral("wayland"))
        || platform.contains(QStringLiteral("treeland"));
    if (!create(preferLayerShell) && preferLayerShell)
        create(false);
    if (!m_window)
        return;

    m_window->setProperty("mode", mode);
    m_window->setProperty("imagePath", imagePath);
    m_window->setProperty("anchorPoint", QVariant::fromValue(anchorPoint));
    m_window->setProperty("externalError", QString());
    // A panel can be created before the asynchronous QML screen binding has
    // resolved; setting screen explicitly keeps multi-monitor placement stable.
    m_window->show();
    m_window->raise();
    m_window->requestActivate();
}

void QuickAiWindowHost::setError(const QString &message) {
    if (m_window)
        m_window->setProperty("externalError", message);
}

void QuickAiWindowHost::close() {
    if (!m_window)
        return;
    QQuickWindow *window = m_window;
    m_window = nullptr;
    window->close();
    window->deleteLater();
}
