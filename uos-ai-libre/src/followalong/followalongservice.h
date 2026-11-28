// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QPoint>
#include <QTimer>
#include <QVariantList>

#include <memory>

class AppController;
class FollowAlongMonitor;
class FollowAlongWindowHost;
class QQmlEngine;

/// AI FollowAlong (划词随航) service.
///
/// It observes the desktop PRIMARY selection, presents UOS AI's compact
/// toolbar near the cursor, and sends the selected text through the existing
/// AppController/ChatModel path.
class FollowAlongService final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    Q_PROPERTY(QVariantList actions READ actions CONSTANT)
    Q_PROPERTY(QVariantList moreActions READ moreActions CONSTANT)

public:
    FollowAlongService(QQmlEngine *engine, AppController *app,
                       QObject *parent = nullptr);
    ~FollowAlongService() override;

    bool enabled() const;
    void setEnabled(bool enabled);
    QVariantList actions() const;
    QVariantList moreActions() const;
    Q_INVOKABLE void ask(const QString &question);
    Q_INVOKABLE void openSettings();
    Q_INVOKABLE void showPreview();
    Q_INVOKABLE void activateToolbar();
    Q_INVOKABLE bool cursorInsideToolbar() const;
    Q_INVOKABLE bool cursorPositionAvailable() const;
    Q_INVOKABLE void setToolbarSize(int width, int height);

    void start();

Q_SIGNALS:
    void enabledChanged();
    void showChatRequested();
    void settingsRequested();

private:
    void onSelectionChanged(const QString &text, const QPoint &globalPos);
    void onSelectionCleared();
    void onActionTriggered(int type, bool custom, const QString &text);
    void showPending();

    AppController *m_app = nullptr;
    std::unique_ptr<FollowAlongMonitor> m_monitor;
    std::unique_ptr<FollowAlongWindowHost> m_window;
    QVariantList m_actions;
    QVariantList m_moreActions;
    QTimer m_showTimer;
    QString m_pendingText;
    QPoint m_pendingPos;
    bool m_enabled = true;
    bool m_started = false;
};
