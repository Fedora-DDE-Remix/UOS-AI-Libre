#pragma once

#include <QObject>
#include <QString>
#include <QTimer>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include "aish-cxxbridge.h"

/// Qt-facing wrapper around the panic-safe Rust `aish-cxxbridge`.
///
/// All Rust calls return JSON strings. This class parses them on the GUI
/// thread, coalesces token deltas at ~30 fps, and exposes Qt signals that QML
/// can bind to. No API key ever crosses into QML when reading configuration.
class AishBridge final : public QObject {
    Q_OBJECT
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(bool generating READ generating NOTIFY generatingChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString sessionId READ sessionId NOTIFY sessionChanged)
    Q_PROPERTY(QString model READ model NOTIFY configChanged)
    Q_PROPERTY(QString apiBase READ apiBase NOTIFY configChanged)
    Q_PROPERTY(bool hasApiKey READ hasApiKey NOTIFY configChanged)
    Q_PROPERTY(QString configPath READ configPath NOTIFY configChanged)
    Q_PROPERTY(QVariantMap config READ config NOTIFY configChanged)
    Q_PROPERTY(QVariantList sessions READ sessions NOTIFY sessionsChanged)
    Q_PROPERTY(QString workspace READ workspace NOTIFY workspaceChanged)
    Q_PROPERTY(QString defaultWorkspace READ defaultWorkspace CONSTANT)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)

public:
    explicit AishBridge(QObject *parent = nullptr);
    ~AishBridge() override;

    bool ready() const;
    bool generating() const;
    QString lastError() const;
    QString sessionId() const;
    QString model() const;
    QString apiBase() const;
    bool hasApiKey() const;
    QString configPath() const;
    QVariantMap config() const;
    QVariantList sessions() const;
    QString workspace() const;
    QString defaultWorkspace() const;
    QString appVersion() const;

    Q_INVOKABLE void initialize();
    Q_INVOKABLE bool createSession(const QString &model);
    Q_INVOKABLE bool sendMessage(const QString &text,
                                 const QVariantList &imagePaths,
                                 const QString &model);
    Q_INVOKABLE void cancel();
    Q_INVOKABLE void approve(const QString &requestId, int decision);
    Q_INVOKABLE void refreshSessions();
    Q_INVOKABLE void loadSession(const QString &sessionId);
    Q_INVOKABLE void deleteSession(const QString &sessionId);
    Q_INVOKABLE void renameSession(const QString &sessionId,
                                   const QString &title);
    Q_INVOKABLE void newConversation();
    Q_INVOKABLE void setWorkspace(const QString &path);
    Q_INVOKABLE void resetWorkspace();
    Q_INVOKABLE void saveModelConfig(const QString &model,
                                     const QString &apiBase,
                                     const QString &apiKey);
    Q_INVOKABLE void openExternal(const QString &url);
    Q_INVOKABLE void openConfigFile();
    Q_INVOKABLE void openWorkspace();
    Q_INVOKABLE void copyText(const QString &text);
    Q_INVOKABLE QString localFilePath(const QUrl &url) const;
    Q_INVOKABLE QString localDirectory(const QUrl &url) const;
    Q_INVOKABLE QString imageUrl(const QString &path) const;
    Q_INVOKABLE QString takeScreenshot();

signals:
    void readyChanged();
    void generatingChanged();
    void lastErrorChanged();
    void sessionChanged();
    void configChanged();
    void sessionsChanged();
    void workspaceChanged();

    void contentDelta(const QString &delta);
    void reasoningStart();
    void reasoningDelta(const QString &delta);
    void reasoningEnd();
    void toolStart(const QVariantMap &tool);
    void toolEnd(const QVariantMap &tool);
    void turnStarted(const QVariantMap &info);
    void turnDone(const QVariantMap &info);
    void turnError(const QString &message);
    void turnCancelled();
    void approvalRequested(const QVariantMap &request);

    void sessionLoaded(const QVariantList &messages, const QString &model);
    void sessionCleared();
    void sessionRenamed(const QString &sessionId, const QString &title);
    void sessionDeleted(const QString &sessionId);
    void toastRequested(const QString &message);
    void errorOccurred(const QString &message);

private:
    BridgeService *service() const;
    bool ensureService();
    void startPolling();
    void pollEvents();
    void handleEvent(const QJsonObject &event);
    void scheduleFlush();
    void flushPending();
    void setReady(bool ready);
    void setGenerating(bool generating);
    void setLastError(const QString &message);
    void applyConfig(const QJsonObject &config);
    void refreshConfig();
    void cleanupOldScreenshots();

    rust::Box<BridgeService> *m_service = nullptr;
    QTimer m_poll_timer;
    QTimer m_flush_timer;
    QString m_pending_content;
    QString m_pending_reasoning;
    QString m_last_error;
    QString m_session_id;
    QString m_workspace;
    QString m_default_workspace;
    QVariantMap m_config;
    QVariantList m_sessions;
    bool m_ready = false;
    bool m_generating = false;
    qint64 m_turn_started_at = 0;
};
