#pragma once

#include <QObject>
#include <QPointer>
#include <QPoint>
#include <QString>

class AppController;
class QQmlComponent;
class QQmlEngine;
class QQuickWindow;

/// Owns the disposable floating quick-answer window.
///
/// On Wayland/Treeland the window uses dde-shell's DLayerShellWindow binding
/// (wlr-layer-shell-unstable-v1). On X11, or when the binding is unavailable,
/// it falls back to a frameless tool window. The chat/session itself stays in
/// AppController; this class only owns the surface lifecycle.
class QuickAiWindowHost final : public QObject {
    Q_OBJECT

public:
    QuickAiWindowHost(QQmlEngine *engine, AppController *app,
                      QObject *parent = nullptr);
    ~QuickAiWindowHost() override;

    void show(int mode, const QString &imagePath, const QPoint &anchorPoint);
    void setError(const QString &message);
    void close();
    bool visible() const;

private:
    bool create(bool preferLayerShell);
    QQmlComponent *loadComponent(bool layerShell);

    QQmlEngine *m_engine = nullptr;
    AppController *m_app = nullptr;
    QQmlComponent *m_layer_component = nullptr;
    QQmlComponent *m_fallback_component = nullptr;
    QPointer<QQuickWindow> m_window;
};
