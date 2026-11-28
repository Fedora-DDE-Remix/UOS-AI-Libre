// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QPoint>
#include <QVariantList>

#include <memory>

class AppController;
class FollowAlongController;
class FollowAlongEventBus;
class FollowAlongEventSource;
class FollowAlongSurface;
class QQmlEngine;

/// AI FollowAlong (划词随航) service.
///
/// This class owns the event bus, the platform event source, the presentation
/// backend and the pure controller. It also routes the toolbar's UI actions
/// into the existing AppController/aish session path; that UI-to-aish glue is
/// intentionally not abstracted.
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

    void start();

Q_SIGNALS:
    void enabledChanged();
    void showChatRequested();
    void settingsRequested();

private:
    void onActionTriggered(int type, bool custom, const QString &text);
    void onAskRequested(const QString &question, const QString &text);

    AppController *m_app = nullptr;
    std::unique_ptr<FollowAlongEventBus> m_bus;
    std::unique_ptr<FollowAlongEventSource> m_source;
    std::unique_ptr<FollowAlongSurface> m_surface;
    std::unique_ptr<FollowAlongController> m_controller;
    QVariantList m_actions;
    QVariantList m_moreActions;
    QString m_previewText;
    QPoint m_previewAnchor;
    bool m_previewExpanded = false;
    bool m_enabled = true;
    bool m_started = false;
    bool m_sourceStarted = false;
};
