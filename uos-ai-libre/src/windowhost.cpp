#include "windowhost.h"

#include "appcontroller.h"

#include <QCoreApplication>
#include <QDebug>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickWindow>
#include <QScreen>
#include <QTimer>

WindowHost::WindowHost(QQmlEngine *engine, AppController *app, QObject *parent)
    : QObject(parent), m_engine(engine), m_app(app) {
    m_component = new QQmlComponent(m_engine, this);
    m_component->loadFromModule(QStringLiteral("UosAiLibre"),
                                QStringLiteral("Main"));
    if (m_component->isError()) {
        qWarning() << "failed to load Main.qml:" << m_component->errorString();
    }
}

bool WindowHost::windowVisible() const {
    return m_window && m_window->isVisible() && m_window->visibility() != QWindow::Minimized;
}

QQuickWindow *WindowHost::window() const { return m_window; }

void WindowHost::showWindow() {
    if (!m_window) {
        createWindow();
        return;
    }
    if (m_window->visibility() == QWindow::Minimized)
        m_window->showNormal();
    else
        m_window->show();
    m_window->raise();
    m_window->requestActivate();
}

void WindowHost::createWindow() {
    if (m_window || m_component->isError())
        return;
    QObject *object = m_component->create(m_engine->rootContext());
    m_window = qobject_cast<QQuickWindow *>(object);
    if (!m_window) {
        if (object)
            object->deleteLater();
        qWarning() << "Main.qml did not produce a QQuickWindow";
        return;
    }

    m_window->setObjectName(QStringLiteral("mainWindow"));
    QSize target = m_last_size.isValid() ? m_last_size
                                         : QSize(1290, 800);
    if (QGuiApplication::platformName() != QStringLiteral("offscreen")) {
        if (QScreen *screen = QGuiApplication::primaryScreen()) {
            const QRect available = screen->availableGeometry();
            target.setWidth(qMax(m_window->minimumWidth(),
                                 qMin(target.width(), available.width())));
            target.setHeight(qMax(m_window->minimumHeight(),
                                  qMin(target.height(), available.height())));
            if (m_has_position)
                m_window->setPosition(m_last_position);
            else
                m_window->setPosition(available.center() -
                                      QPoint(target.width() / 2,
                                             target.height() / 2));
        }
    }
    m_window->resize(target);

    connect(m_window, &QQuickWindow::closing, this,
            [this](QQuickCloseEvent *) {
                if (m_quitting)
                    return;
                if (m_app && m_app->savingSession()) {
                    m_quitting = true;
                    QCoreApplication::quit();
                    return;
                }
                windowClosed();
            });
    connect(m_window, &QWindow::visibleChanged, this,
            &WindowHost::windowVisibleChanged);
    connect(m_window, &QWindow::visibilityChanged, this,
            &WindowHost::windowVisibleChanged);
    connect(m_window, &QObject::destroyed, this, [this]() {
        m_window = nullptr;
        emit windowVisibleChanged();
    });
    m_window->show();
    m_window->raise();
    m_window->requestActivate();
    emit windowVisibleChanged();
}

void WindowHost::windowClosed() {
    if (!m_window || m_quitting)
        return;
    m_last_size = m_window->size();
    m_last_position = m_window->position();
    m_has_position = true;
    QQuickWindow *window = m_window;
    m_window = nullptr;
    window->deleteLater();
    emit windowVisibleChanged();
}

void WindowHost::newChat() {
    showWindow();
    QTimer::singleShot(0, this, [this]() {
        if (m_window)
            QMetaObject::invokeMethod(m_window, "newChatFromTray");
    });
}

void WindowHost::quitApp() {
    m_quitting = true;
    QCoreApplication::quit();
}
