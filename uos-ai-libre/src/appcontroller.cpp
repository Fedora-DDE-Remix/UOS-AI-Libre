#include "appcontroller.h"

#include <QCoreApplication>
#include <QDesktopServices>
#include <QFileInfo>
#include <QFont>
#include <QGuiApplication>
#include <QSettings>
#include <QStyleHints>
#include <QUrl>

AppController::AppController(QObject *parent)
    : QObject(parent), m_chat(this) {
    wire();
    m_model = m_bridge.model();
    m_title_cache = sessionTitle();
    m_theme_mode = QSettings().value(QStringLiteral("theme/mode"), 2).toInt();
    if (m_theme_mode < 0 || m_theme_mode > 2)
        m_theme_mode = 2;
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
            [this](Qt::ColorScheme) { emit systemDarkChanged(); });
    m_approval_timeout.setSingleShot(true);
    m_approval_timeout.setInterval(290000);
    connect(&m_approval_timeout, &QTimer::timeout, this,
            [this]() { resolveApproval(3); });
    m_interaction_timeout.setSingleShot(true);
    m_interaction_timeout.setInterval(290000);
    connect(&m_interaction_timeout, &QTimer::timeout, this, [this]() {
        resolveInteraction({{QStringLiteral("cancelled"), true}});
    });
}

void AppController::wire() {
    connect(&m_bridge, &AishBridge::readyChanged, this,
            &AppController::readyChanged);
    connect(&m_bridge, &AishBridge::generatingChanged, this,
            &AppController::generatingChanged);
    connect(&m_bridge, &AishBridge::lastErrorChanged, this,
            &AppController::lastErrorChanged);
    connect(&m_bridge, &AishBridge::errorOccurred, this,
            &AppController::errorOccurred);
    connect(&m_bridge, &AishBridge::sessionChanged, this, [this]() {
        recomputeTitle();
        emit sessionChanged();
    });
    connect(&m_bridge, &AishBridge::configChanged, this,
            &AppController::onConfigChanged);
    connect(&m_bridge, &AishBridge::sessionsChanged, this,
            &AppController::onSessionsChanged);
    connect(&m_bridge, &AishBridge::workspaceChanged, this,
            &AppController::workspaceChanged);
    connect(&m_bridge, &AishBridge::toastRequested, this,
            &AppController::toastRequested);
    connect(&m_bridge, &AishBridge::approvalRequested, this,
            &AppController::enqueueApproval);
    connect(&m_bridge, &AishBridge::interactionRequested, this,
            &AppController::enqueueInteraction);
    connect(&m_bridge, &AishBridge::turnStarted, this,
            &AppController::onTurnStarted);
    connect(&m_bridge, &AishBridge::contentDelta, this,
            &AppController::onContentDelta);
    connect(&m_bridge, &AishBridge::reasoningStart, this,
            &AppController::onReasoningStart);
    connect(&m_bridge, &AishBridge::reasoningDelta, this,
            &AppController::onReasoningDelta);
    connect(&m_bridge, &AishBridge::reasoningEnd, this,
            &AppController::onReasoningEnd);
    connect(&m_bridge, &AishBridge::toolStart, this,
            &AppController::onToolStart);
    connect(&m_bridge, &AishBridge::toolEnd, this,
            &AppController::onToolEnd);
    connect(&m_bridge, &AishBridge::turnDone, this,
            &AppController::onTurnDone);
    connect(&m_bridge, &AishBridge::turnError, this,
            &AppController::onTurnError);
    connect(&m_bridge, &AishBridge::turnCancelled, this,
            &AppController::onTurnCancelled);
    connect(&m_bridge, &AishBridge::sessionLoaded, this,
            &AppController::onSessionLoaded);
    connect(&m_bridge, &AishBridge::sessionCleared, this,
            &AppController::onSessionCleared);
}

ChatModel *AppController::chat() { return &m_chat; }
bool AppController::ready() const { return m_bridge.ready(); }

bool AppController::systemDark() const {
    return QGuiApplication::styleHints()->colorScheme() ==
           Qt::ColorScheme::Dark;
}

bool AppController::savingSession() const { return m_saving_session; }

void AppController::markSavingSession() {
    if (m_saving_session)
        return;
    m_saving_session = true;
    emit savingSessionChanged();
}

int AppController::themeMode() const { return m_theme_mode; }

void AppController::setThemeMode(int mode) {
    mode = qBound(0, mode, 2);
    if (mode == m_theme_mode)
        return;
    m_theme_mode = mode;
    QSettings().setValue(QStringLiteral("theme/mode"), mode);
    emit themeModeChanged();
}
bool AppController::generating() const { return m_bridge.generating(); }
QString AppController::lastError() const { return m_bridge.lastError(); }
QString AppController::sessionId() const { return m_bridge.sessionId(); }
QString AppController::model() const { return m_model; }
QString AppController::apiBase() const { return m_bridge.apiBase(); }
bool AppController::hasApiKey() const { return m_bridge.hasApiKey(); }
QString AppController::configPath() const { return m_bridge.configPath(); }
QVariantMap AppController::config() const { return m_bridge.config(); }

QVariantMap AppController::pendingApproval() const { return m_pending_approval; }

int AppController::pendingApprovalCount() const {
    return m_pending_approval.isEmpty()
               ? 0
               : 1 + int(m_approval_queue.size());
}
QVariantMap AppController::pendingInteraction() const {
    return m_pending_interaction;
}

int AppController::pendingInteractionCount() const {
    return m_pending_interaction.isEmpty()
               ? 0
               : 1 + int(m_interaction_queue.size());
}

QVariantMap AppController::policyInfo() const { return m_policy_info; }
QVariantList AppController::toolCatalog() const { return m_tool_catalog; }
QVariantMap AppController::skillCatalog() const { return m_skill_catalog; }
QVariantMap AppController::mcpStatus() const { return m_mcp_status; }
QVariantList AppController::memoryEntries() const { return m_memory_entries; }
QVariantList AppController::auditEvents() const { return m_audit_events; }

QVariantList AppController::sessions() const { return m_bridge.sessions(); }
QString AppController::workspace() const { return m_bridge.workspace(); }
QString AppController::defaultWorkspace() const {
    return m_bridge.defaultWorkspace();
}
QString AppController::appVersion() const {
    return QCoreApplication::applicationVersion();
}

QString AppController::fontFamily() const {
    return QGuiApplication::font().family();
}

QString AppController::sessionTitle() const {
    const QString current_id = m_bridge.sessionId();
    if (!current_id.isEmpty()) {
        const QVariantList list = m_bridge.sessions();
        for (const QVariant &entry : list) {
            const QVariantMap session = entry.toMap();
            if (session.value(QStringLiteral("session_id")).toString() ==
                current_id) {
                const QString summary =
                    session.value(QStringLiteral("summary")).toString();
                if (!summary.trimmed().isEmpty())
                    return summary;
            }
        }
    }
    if (!m_local_title.trimmed().isEmpty())
        return m_local_title;
    return QStringLiteral("新对话");
}

void AppController::recomputeTitle() {
    const QString next = sessionTitle();
    if (next == m_title_cache)
        return;
    m_title_cache = next;
    emit sessionTitleChanged();
}

void AppController::initialize() {
    const QString before = m_bridge.lastError();
    const QString old_model = m_model;
    m_bridge.initialize();
    if (m_model.isEmpty())
        m_model = m_bridge.model();
    if (m_model != old_model)
        emit modelChanged();
    recomputeTitle();
    if (!m_bridge.lastError().isEmpty() && m_bridge.lastError() != before)
        emit errorOccurred(m_bridge.lastError());
    if (m_bridge.ready()) {
        refreshPolicy();
        refreshToolCatalog();
        refreshSkillCatalog();
        refreshMcpStatus();
        refreshMemory();
        refreshAudit();
    }
}

bool AppController::sendMessage(const QString &text,
                               const QVariantList &imagePaths) {
    if (!m_bridge.sendMessage(text, imagePaths, m_model))
        return false;

    QVariantList attachments;
    attachments.reserve(imagePaths.size());
    for (const QVariant &entry : imagePaths) {
        const QString path = entry.toString();
        QVariantMap attachment;
        attachment.insert(QStringLiteral("path"), path);
        attachment.insert(QStringLiteral("previewUrl"), m_bridge.imageUrl(path));
        attachments.append(attachment);
    }
    m_chat.appendUser(text, attachments);
    if (m_local_title.trimmed().isEmpty()) {
        m_local_title = text.trimmed().isEmpty()
                            ? QStringLiteral("图片对话")
                            : text.trimmed();
        recomputeTitle();
    }
    m_assistant_open = false;
    return true;
}

void AppController::stop() { m_bridge.cancel(); }

void AppController::newConversation() {
    m_bridge.newConversation();
}

void AppController::loadSession(const QString &sessionId) {
    const QString before = m_bridge.lastError();
    m_bridge.loadSession(sessionId);
    if (!m_bridge.lastError().isEmpty() && m_bridge.lastError() != before)
        emit toastRequested(m_bridge.lastError());
}

void AppController::deleteSession(const QString &sessionId) {
    const QString before = m_bridge.lastError();
    m_bridge.deleteSession(sessionId);
    if (!m_bridge.lastError().isEmpty() && m_bridge.lastError() != before)
        emit toastRequested(m_bridge.lastError());
}

void AppController::renameSession(const QString &sessionId,
                                  const QString &title) {
    const QString before = m_bridge.lastError();
    m_bridge.renameSession(sessionId, title);
    if (!m_bridge.lastError().isEmpty() && m_bridge.lastError() != before)
        emit toastRequested(m_bridge.lastError());
}

void AppController::refreshSessions() { m_bridge.refreshSessions(); }

void AppController::saveModelConfig(const QString &model,
                                    const QString &apiBase,
                                    const QString &apiKey) {
    const QString before = m_bridge.lastError();
    m_bridge.saveModelConfig(model, apiBase, apiKey);
    if (!m_bridge.lastError().isEmpty() && m_bridge.lastError() != before)
        emit toastRequested(m_bridge.lastError());
}

void AppController::resolveApproval(int decision) {
    if (m_pending_approval.isEmpty())
        return;
    const QString request_id =
        m_pending_approval.value(QStringLiteral("request_id")).toString();
    m_pending_approval.clear();
    if (!request_id.isEmpty())
        m_bridge.approve(request_id, decision);
    if (!m_approval_queue.isEmpty())
        m_pending_approval = m_approval_queue.takeFirst();
    if (m_pending_approval.isEmpty())
        m_approval_timeout.stop();
    else
        m_approval_timeout.start();
    emit pendingApprovalChanged();
}

void AppController::resolveInteraction(const QVariantMap &response) {
    if (m_pending_interaction.isEmpty())
        return;
    const QString request_id =
        m_pending_interaction.value(QStringLiteral("request_id")).toString();
    m_pending_interaction.clear();
    if (!request_id.isEmpty())
        m_bridge.respondInteraction(request_id, response);
    if (!m_interaction_queue.isEmpty())
        m_pending_interaction = m_interaction_queue.takeFirst();
    if (m_pending_interaction.isEmpty())
        m_interaction_timeout.stop();
    else
        m_interaction_timeout.start();
    emit pendingInteractionChanged();
}

void AppController::refreshPolicy() {
    const QVariantMap info = m_bridge.policyInfo();
    if (info.isEmpty())
        return;
    m_policy_info = info;
    emit policyInfoChanged();
}

QVariantMap AppController::savePolicy(const QVariantMap &fields) {
    const QVariantMap result = m_bridge.savePolicy(fields);
    if (result.value(QStringLiteral("ok"), false).toBool()) {
        m_policy_info = result;
        emit policyInfoChanged();
        emit toastRequested(QStringLiteral("aish 安全策略已保存"));
    } else {
        const QString error = result.value(QStringLiteral("error")).toString();
        emit toastRequested(error.isEmpty() ? QStringLiteral("安全策略保存失败")
                                            : error);
    }
    return result;
}

void AppController::refreshToolCatalog() {
    const QVariantList tools = m_bridge.toolCatalog();
    if (tools == m_tool_catalog)
        return;
    m_tool_catalog = tools;
    emit toolCatalogChanged();
}

void AppController::refreshSkillCatalog() {
    const QVariantMap catalog = m_bridge.skillCatalog();
    if (catalog.isEmpty())
        return;
    m_skill_catalog = catalog;
    emit skillCatalogChanged();
}

void AppController::trustSkill(const QString &name) {
    const QVariantMap result = m_bridge.trustSkill(name);
    if (result.value(QStringLiteral("ok"), false).toBool()) {
        emit toastRequested(result.value(QStringLiteral("message")).toString());
        refreshSkillCatalog();
    } else {
        emit toastRequested(result.value(QStringLiteral("error")).toString());
    }
}

void AppController::refreshMcpStatus() {
    const QVariantMap status = m_bridge.mcpStatus();
    if (status.isEmpty())
        return;
    m_mcp_status = status;
    emit mcpStatusChanged();
}

void AppController::reloadMcp() {
    const QVariantMap status = m_bridge.reloadMcp();
    if (status.isEmpty())
        return;
    m_mcp_status = status;
    emit mcpStatusChanged();
    emit toastRequested(QStringLiteral("MCP 连接已重载"));
}

void AppController::refreshMemory() {
    const QVariantList entries = m_bridge.memoryList();
    if (entries == m_memory_entries)
        return;
    m_memory_entries = entries;
    emit memoryEntriesChanged();
}

void AppController::refreshAudit() {
    const QVariantList events = m_bridge.auditTail();
    if (events == m_audit_events)
        return;
    m_audit_events = events;
    emit auditEventsChanged();
}

void AppController::openPath(const QString &path) {
    if (path.isEmpty())
        return;
    const QFileInfo info(path);
    if (!info.exists()) {
        emit toastRequested(QStringLiteral("路径不存在：%1").arg(path));
        return;
    }
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(info.absoluteFilePath())))
        emit toastRequested(QStringLiteral("无法打开路径"));
}

void AppController::setWorkspace(const QString &path) {
    const QString before = m_bridge.lastError();
    m_bridge.setWorkspace(path);
    if (!m_bridge.lastError().isEmpty() && m_bridge.lastError() != before)
        emit toastRequested(m_bridge.lastError());
}

void AppController::resetWorkspace() {
    const QString before = m_bridge.lastError();
    m_bridge.resetWorkspace();
    if (!m_bridge.lastError().isEmpty() && m_bridge.lastError() != before)
        emit toastRequested(m_bridge.lastError());
}

void AppController::openConfigFile() {
    const QString before = m_bridge.lastError();
    m_bridge.openConfigFile();
    if (!m_bridge.lastError().isEmpty() && m_bridge.lastError() != before)
        emit toastRequested(m_bridge.lastError());
}
void AppController::openWorkspace() { m_bridge.openWorkspace(); }
void AppController::openExternal(const QString &url) {
    m_bridge.openExternal(url);
}
void AppController::copyText(const QString &text) { m_bridge.copyText(text); }

QString AppController::localFilePath(const QUrl &url) const {
    return m_bridge.localFilePath(url);
}
QString AppController::localDirectory(const QUrl &url) const {
    return m_bridge.localDirectory(url);
}
QString AppController::imageUrl(const QString &path) const {
    return m_bridge.imageUrl(path);
}
QString AppController::takeScreenshot() { return m_bridge.takeScreenshot(); }

void AppController::showToast(const QString &message) {
    emit toastRequested(message);
}

void AppController::beginAssistantIfNeeded(const QString &model) {
    if (m_assistant_open)
        return;
    m_chat.beginAssistant(model.isEmpty() ? m_model : model);
    m_assistant_open = true;
}

void AppController::onTurnStarted(const QVariantMap &info) {
    const QString turn_model = info.value(QStringLiteral("model")).toString();
    if (!turn_model.isEmpty() && turn_model != m_model) {
        m_model = turn_model;
        emit modelChanged();
    }
    beginAssistantIfNeeded(m_model);
}

void AppController::onContentDelta(const QString &delta) {
    beginAssistantIfNeeded();
    m_chat.appendContent(delta);
}

void AppController::onReasoningStart() {
    beginAssistantIfNeeded();
    m_chat.beginReasoning();
}

void AppController::onReasoningDelta(const QString &delta) {
    beginAssistantIfNeeded();
    m_chat.appendReasoning(delta);
}

void AppController::onReasoningEnd() {
    if (m_assistant_open)
        m_chat.endReasoning();
}

void AppController::onToolStart(const QVariantMap &tool) {
    beginAssistantIfNeeded();
    m_chat.addToolStart(tool);
}

void AppController::onToolEnd(const QVariantMap &tool) {
    beginAssistantIfNeeded();
    m_chat.addToolEnd(tool);
}

void AppController::onTurnDone(const QVariantMap &info) {
    beginAssistantIfNeeded();
    m_chat.setContent(info.value(QStringLiteral("text")).toString());
    m_chat.finishAssistant();
    m_assistant_open = false;
}

void AppController::onTurnError(const QString &message) {
    beginAssistantIfNeeded();
    m_chat.failAssistant(message);
    m_assistant_open = false;
}

void AppController::onTurnCancelled() {
    if (m_assistant_open)
        m_chat.cancelAssistant();
    m_assistant_open = false;
}

void AppController::onSessionLoaded(const QVariantList &messages,
                                    const QString &model) {
    QVariantList visible = messages;
    if (visible.size() > 300) {
        int start = visible.size() - 300;
        while (start < visible.size() &&
               visible.at(start).toMap().value(QStringLiteral("role")).toString() !=
                   QStringLiteral("user"))
            ++start;
        visible = visible.mid(start);
    }
    m_chat.loadTranscript(visible);
    m_assistant_open = false;
    m_model = model.isEmpty() ? m_bridge.model() : model;
    emit modelChanged();
    m_local_title.clear();
    recomputeTitle();
    emit sessionLoaded();
}

void AppController::enqueueApproval(const QVariantMap &request) {
    if (request.value(QStringLiteral("request_id")).toString().isEmpty())
        return;
    if (m_pending_approval.isEmpty()) {
        m_pending_approval = request;
        m_approval_timeout.start();
    } else {
        m_approval_queue.append(request);
    }
    emit pendingApprovalChanged();
}

void AppController::enqueueInteraction(const QVariantMap &request) {
    if (request.value(QStringLiteral("request_id")).toString().isEmpty())
        return;
    if (m_pending_interaction.isEmpty()) {
        m_pending_interaction = request;
        m_interaction_timeout.start();
    } else {
        m_interaction_queue.append(request);
    }
    emit pendingInteractionChanged();
}

void AppController::onSessionCleared() {
    if (!m_pending_approval.isEmpty()) {
        const QString request_id =
            m_pending_approval.value(QStringLiteral("request_id")).toString();
        if (!request_id.isEmpty())
            m_bridge.approve(request_id, 3);
    }
    m_pending_approval.clear();
    m_approval_queue.clear();
    m_approval_timeout.stop();
    emit pendingApprovalChanged();
    m_chat.clear();
    m_assistant_open = false;
    m_model = m_bridge.model();
    emit modelChanged();
    m_local_title.clear();
    recomputeTitle();
    emit sessionCleared();
}

void AppController::onConfigChanged() {
    emit configChanged();
    if (m_bridge.sessionId().isEmpty() || m_model.isEmpty()) {
        const QString next = m_bridge.model();
        if (next != m_model) {
            m_model = next;
            emit modelChanged();
        }
    }
}

void AppController::onSessionsChanged() {
    emit sessionsChanged();
    recomputeTitle();
}
