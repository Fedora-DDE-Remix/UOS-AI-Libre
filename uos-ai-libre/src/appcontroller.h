#pragma once

#include <QObject>
#include <QTimer>
#include <QString>
#include <QUrl>
#include <QVariantList>
#include <QVariantMap>

#include "aishbridge.h"
#include "chatmodel.h"

/// Application service layer.
///
/// The QML pages only talk to this object. `AishBridge` owns the FFI/JSON
/// boundary and `ChatModel` owns presentation rows; neither the Rust bridge nor
/// aish's internal event schema is exposed to the UI. This keeps UI changes
/// independent from the agent backend and makes the permission/approval flow a
/// plain Qt signal/slot contract.
class AppController final : public QObject {
    Q_OBJECT
    Q_PROPERTY(ChatModel *chat READ chat CONSTANT)
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(bool systemDark READ systemDark NOTIFY systemDarkChanged)
    Q_PROPERTY(bool savingSession READ savingSession NOTIFY savingSessionChanged)
    Q_PROPERTY(int themeMode READ themeMode WRITE setThemeMode NOTIFY themeModeChanged)
    Q_PROPERTY(bool generating READ generating NOTIFY generatingChanged)
    Q_PROPERTY(QString lastError READ lastError NOTIFY lastErrorChanged)
    Q_PROPERTY(QString sessionId READ sessionId NOTIFY sessionChanged)
    Q_PROPERTY(QString sessionTitle READ sessionTitle NOTIFY sessionTitleChanged)
    Q_PROPERTY(QString model READ model NOTIFY modelChanged)
    Q_PROPERTY(QString apiBase READ apiBase NOTIFY configChanged)
    Q_PROPERTY(bool hasApiKey READ hasApiKey NOTIFY configChanged)
    Q_PROPERTY(QString configPath READ configPath NOTIFY configChanged)
    Q_PROPERTY(QVariantMap config READ config NOTIFY configChanged)
    Q_PROPERTY(QVariantMap pendingApproval READ pendingApproval NOTIFY pendingApprovalChanged)
    Q_PROPERTY(int pendingApprovalCount READ pendingApprovalCount NOTIFY pendingApprovalChanged)
    Q_PROPERTY(QVariantMap pendingInteraction READ pendingInteraction NOTIFY pendingInteractionChanged)
    Q_PROPERTY(int pendingInteractionCount READ pendingInteractionCount NOTIFY pendingInteractionChanged)
    Q_PROPERTY(QVariantMap policyInfo READ policyInfo NOTIFY policyInfoChanged)
    Q_PROPERTY(QVariantList sessionApprovals READ sessionApprovals NOTIFY sessionApprovalsChanged)
    Q_PROPERTY(QVariantList toolCatalog READ toolCatalog NOTIFY toolCatalogChanged)
    Q_PROPERTY(QVariantMap skillCatalog READ skillCatalog NOTIFY skillCatalogChanged)
    Q_PROPERTY(bool skillSearchBusy READ skillSearchBusy NOTIFY skillSearchBusyChanged)
    Q_PROPERTY(bool skillInstallBusy READ skillInstallBusy NOTIFY skillInstallBusyChanged)
    Q_PROPERTY(QVariantMap skillSearchResult READ skillSearchResult NOTIFY skillSearchResultChanged)
    Q_PROPERTY(QVariantMap skillInstallResult READ skillInstallResult NOTIFY skillInstallResultChanged)
    Q_PROPERTY(QVariantMap skillReview READ skillReview NOTIFY skillReviewChanged)
    Q_PROPERTY(QVariantMap mcpStatus READ mcpStatus NOTIFY mcpStatusChanged)
    Q_PROPERTY(bool mcpBusy READ mcpBusy NOTIFY mcpBusyChanged)
    Q_PROPERTY(QVariantMap mcpTestResult READ mcpTestResult NOTIFY mcpTestResultChanged)
    Q_PROPERTY(QVariantList memoryEntries READ memoryEntries NOTIFY memoryEntriesChanged)
    Q_PROPERTY(QVariantList auditEvents READ auditEvents NOTIFY auditEventsChanged)
    Q_PROPERTY(QVariantList sessions READ sessions NOTIFY sessionsChanged)
    Q_PROPERTY(QString workspace READ workspace NOTIFY workspaceChanged)
    Q_PROPERTY(QString defaultWorkspace READ defaultWorkspace CONSTANT)
    Q_PROPERTY(QString appVersion READ appVersion CONSTANT)
    Q_PROPERTY(QString fontFamily READ fontFamily CONSTANT)

public:
    explicit AppController(QObject *parent = nullptr);

    ChatModel *chat();
    bool ready() const;
    bool systemDark() const;
    bool savingSession() const;
    Q_INVOKABLE void markSavingSession();
    int themeMode() const;
    void setThemeMode(int mode);
    bool generating() const;
    QString lastError() const;
    QString sessionId() const;
    QString sessionTitle() const;
    QString model() const;
    QString apiBase() const;
    bool hasApiKey() const;
    QString configPath() const;
    QVariantMap config() const;
    QVariantMap pendingApproval() const;
    int pendingApprovalCount() const;
    QVariantMap pendingInteraction() const;
    int pendingInteractionCount() const;
    QVariantMap policyInfo() const;
    QVariantList sessionApprovals() const;
    QVariantList toolCatalog() const;
    QVariantMap skillCatalog() const;
    bool skillSearchBusy() const;
    bool skillInstallBusy() const;
    QVariantMap skillSearchResult() const;
    QVariantMap skillInstallResult() const;
    QVariantMap skillReview() const;
    QVariantMap mcpStatus() const;
    bool mcpBusy() const;
    QVariantMap mcpTestResult() const;
    QVariantList memoryEntries() const;
    QVariantList auditEvents() const;
    QVariantList sessions() const;
    QString workspace() const;
    QString defaultWorkspace() const;
    QString appVersion() const;
    QString fontFamily() const;

    Q_INVOKABLE void initialize();
    Q_INVOKABLE bool sendMessage(const QString &text,
                                 const QVariantList &imagePaths);
    Q_INVOKABLE void stop();
    Q_INVOKABLE void newConversation();
    Q_INVOKABLE void loadSession(const QString &sessionId);
    Q_INVOKABLE void deleteSession(const QString &sessionId);
    Q_INVOKABLE void renameSession(const QString &sessionId,
                                   const QString &title);
    Q_INVOKABLE void refreshSessions();
    Q_INVOKABLE void saveModelConfig(const QString &model,
                                     const QString &apiBase,
                                     const QString &apiKey);
    Q_INVOKABLE void resolveApproval(int decision);
    Q_INVOKABLE void resolveInteraction(const QVariantMap &response);
    Q_INVOKABLE void refreshPolicy();
    Q_INVOKABLE QVariantMap savePolicy(const QVariantMap &fields);
    Q_INVOKABLE QVariantMap savePolicyRules(const QVariantList &rules);
    Q_INVOKABLE QVariantMap previewPolicy(const QString &command,
                                          const QVariantList &draftRules);
    Q_INVOKABLE void refreshSessionApprovals();
    Q_INVOKABLE void revokeSessionApproval(const QString &kind,
                                           const QString &first,
                                           const QString &second);
    Q_INVOKABLE void clearSessionApprovals();
    Q_INVOKABLE void refreshToolCatalog();
    Q_INVOKABLE void refreshSkillCatalog();
    Q_INVOKABLE void searchSkills(const QString &query);
    Q_INVOKABLE void installSkill(const QString &skillId);
    Q_INVOKABLE void reviewSkill(const QString &name);
    Q_INVOKABLE void trustSkill(const QString &name);
    Q_INVOKABLE void removeSkill(const QString &name);
    Q_INVOKABLE void refreshMcpStatus();
    Q_INVOKABLE void reloadMcp();
    Q_INVOKABLE void addMcpServer(const QVariantMap &args);
    Q_INVOKABLE void updateMcpServer(const QString &name,
                                     const QVariantMap &args);
    Q_INVOKABLE void removeMcpServer(const QString &name);
    Q_INVOKABLE void setMcpMasterEnabled(bool enabled);
    Q_INVOKABLE void setMcpEnabled(const QString &name, bool enabled);
    Q_INVOKABLE void setMcpTrust(const QString &name,
                                 const QString &tool,
                                 const QString &level);
    Q_INVOKABLE void testMcpServer(const QString &name);
    Q_INVOKABLE void testMcpDraft(const QVariantMap &args);
    Q_INVOKABLE void refreshMemory();
    Q_INVOKABLE void refreshAudit();
    Q_INVOKABLE void openPath(const QString &path);
    Q_INVOKABLE void setWorkspace(const QString &path);
    Q_INVOKABLE void resetWorkspace();
    Q_INVOKABLE void openConfigFile();
    Q_INVOKABLE void openWorkspace();
    Q_INVOKABLE void openExternal(const QString &url);
    Q_INVOKABLE void copyText(const QString &text);
    Q_INVOKABLE QString localFilePath(const QUrl &url) const;
    Q_INVOKABLE QString localDirectory(const QUrl &url) const;
    Q_INVOKABLE QString imageUrl(const QString &path) const;
    Q_INVOKABLE QString takeScreenshot();
    Q_INVOKABLE void showToast(const QString &message);

signals:
    void readyChanged();
    void systemDarkChanged();
    void savingSessionChanged();
    void themeModeChanged();
    void generatingChanged();
    void lastErrorChanged();
    void sessionChanged();
    void sessionTitleChanged();
    void modelChanged();
    void configChanged();
    void sessionsChanged();
    void workspaceChanged();
    void toastRequested(const QString &message);
    void pendingApprovalChanged();
    void pendingInteractionChanged();
    void policyInfoChanged();
    void sessionApprovalsChanged();
    void toolCatalogChanged();
    void skillCatalogChanged();
    void skillSearchBusyChanged();
    void skillInstallBusyChanged();
    void skillSearchResultChanged();
    void skillInstallResultChanged();
    void skillReviewChanged();
    void mcpStatusChanged();
    void mcpBusyChanged();
    void mcpTestResultChanged();
    void skillSearchFinished(const QVariantMap &result);
    void skillInstallFinished(const QVariantMap &result);
    void skillReviewFinished(const QVariantMap &result);
    void mcpTestFinished(const QVariantMap &result);
    void memoryEntriesChanged();
    void auditEventsChanged();
    void sessionLoaded();
    void sessionCleared();
    void errorOccurred(const QString &message);

private:
    void wire();
    void recomputeTitle();
    void beginAssistantIfNeeded(const QString &model = {});
    void onTurnStarted(const QVariantMap &info);
    void onContentDelta(const QString &delta);
    void onReasoningStart();
    void onReasoningDelta(const QString &delta);
    void onReasoningEnd();
    void onToolStart(const QVariantMap &tool);
    void onToolEnd(const QVariantMap &tool);
    void onTurnDone(const QVariantMap &info);
    void onTurnError(const QString &message);
    void onTurnCancelled();
    void onSessionLoaded(const QVariantList &messages, const QString &model);
    void onSessionCleared();
    void enqueueApproval(const QVariantMap &request);
    void enqueueInteraction(const QVariantMap &request);
    void onConfigChanged();
    void onSessionsChanged();
    void onSettingsJobFinished(const QVariantMap &result);

    AishBridge m_bridge;
    ChatModel m_chat;
    QString m_model;
    QString m_local_title;
    QString m_title_cache;
    QVariantMap m_pending_approval;
    QList<QVariantMap> m_approval_queue;
    QTimer m_approval_timeout;
    QVariantMap m_pending_interaction;
    QList<QVariantMap> m_interaction_queue;
    QTimer m_interaction_timeout;
    QVariantMap m_policy_info;
    QVariantList m_session_approvals;
    QVariantList m_tool_catalog;
    QVariantMap m_skill_catalog;
    QVariantMap m_skill_search_result;
    QVariantMap m_skill_install_result;
    QVariantMap m_skill_review;
    QVariantMap m_mcp_status;
    QVariantMap m_mcp_test_result;
    bool m_skill_search_busy = false;
    bool m_skill_install_busy = false;
    bool m_mcp_busy = false;
    QVariantList m_memory_entries;
    QVariantList m_audit_events;
    int m_theme_mode = 2;
    bool m_saving_session = false;
    bool m_assistant_open = false;
};
