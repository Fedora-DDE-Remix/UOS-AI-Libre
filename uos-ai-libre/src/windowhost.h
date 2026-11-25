#pragma once

#include <QObject>
#include <QPoint>
#include <QSize>
#include <QString>

class AppController;
class QQmlComponent;
class QQmlEngine;
class QQuickWindow;

/// Owns the disposable main window while the agent backend and tray stay alive.
///
/// Closing the window deletes the QML window (and its QQuickWindow), but not
/// `AppController`/`AishBridge`. Showing it again creates a fresh window that
/// rebinds to the existing ChatModel, so a streaming turn can finish while the
/// window is closed. This is also the safe behavior on Wayland, where hiding and
/// re-showing a native window can destroy its platform surface.
class WindowHost final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool windowVisible READ windowVisible NOTIFY windowVisibleChanged)

public:
    WindowHost(QQmlEngine *engine, AppController *app,
               QObject *parent = nullptr);

    bool windowVisible() const;
    QQuickWindow *window() const;

    Q_INVOKABLE void showWindow();
    Q_INVOKABLE void windowClosed();
    Q_INVOKABLE void newChat();
    Q_INVOKABLE void quitApp();

signals:
    void windowVisibleChanged();

private:
    void createWindow();

    QQmlEngine *m_engine = nullptr;
    AppController *m_app = nullptr;
    QQmlComponent *m_component = nullptr;
    QQuickWindow *m_window = nullptr;
    QSize m_last_size;
    QPoint m_last_position;
    bool m_has_position = false;
    bool m_quitting = false;
};
