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
    m_provider_labels =
        QSettings().value(QStringLiteral("model/providerLabels")).toMap();
    m_provider_models =
        QSettings().value(QStringLiteral("model/providerModels")).toMap();
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
    connect(&m_bridge, &AishBridge::settingsJobFinished, this,
            &AppController::onSettingsJobFinished);
    connect(&m_bridge, &AishBridge::sessionChanged, this,
            [this]() { refreshSessionApprovals(); });
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

int AppController::historyTotal() const { return m_history_total; }
int AppController::historyStart() const { return m_history_start; }
bool AppController::historyLoading() const { return m_history_loading; }
bool AppController::hasOlderHistory() const { return m_history_start > 0; }

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
QVariantList AppController::sessionApprovals() const { return m_session_approvals; }
QVariantList AppController::toolCatalog() const { return m_tool_catalog; }
QVariantMap AppController::skillCatalog() const { return m_skill_catalog; }
bool AppController::skillSearchBusy() const { return m_skill_search_busy; }
bool AppController::skillInstallBusy() const { return m_skill_install_busy; }
QVariantMap AppController::skillSearchResult() const { return m_skill_search_result; }
QVariantMap AppController::skillInstallResult() const { return m_skill_install_result; }
QVariantMap AppController::skillReview() const { return m_skill_review; }
QVariantMap AppController::mcpStatus() const { return m_mcp_status; }
bool AppController::mcpBusy() const { return m_mcp_busy; }
QVariantMap AppController::mcpTestResult() const { return m_mcp_test_result; }
QVariantList AppController::memoryEntries() const { return m_memory_entries; }
QVariantList AppController::auditEvents() const { return m_audit_events; }

QVariantList AppController::providers() const { return m_providers; }
QVariantList AppController::chatModels() const { return m_chat_models; }
bool AppController::modelFetchBusy() const { return m_model_fetch_busy; }
QVariantMap AppController::modelFetchResult() const { return m_model_fetch_result; }
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
        refreshProviders();
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

QString AppController::providerLabel(const QString &apiBase) const {
    return m_provider_labels.value(apiBase).toString();
}

void AppController::setProviderLabel(const QString &apiBase, const QString &label) {
    if (apiBase.isEmpty() || label.trimmed().isEmpty())
        return;
    m_provider_labels.insert(apiBase, label.trimmed());
    QSettings().setValue(QStringLiteral("model/providerLabels"),
                         m_provider_labels);
}

QVariantList AppController::providerModels(const QString &apiBase) const {
    return m_provider_models.value(apiBase).toList();
}

void AppController::setProviderModelsStore(const QString &apiBase,
                                           const QVariantList &models) {
    if (apiBase.isEmpty())
        return;
    QStringList unique;
    for (const QVariant &entry : models) {
        const QString value = entry.toString().trimmed();
        if (!value.isEmpty() && !unique.contains(value))
            unique.append(value);
        if (unique.size() >= 100)
            break;
    }
    QVariantList stored;
    for (const QString &value : unique)
        stored.append(value);
    m_provider_models.insert(apiBase, stored);
    QSettings().setValue(QStringLiteral("model/providerModels"),
                         m_provider_models);
}

void AppController::refreshProviders() {
    const QVariantMap info = m_bridge.providersInfo();
    if (info.isEmpty())
        return;
    QVariantList providers = info.value(QStringLiteral("providers")).toList();
    QVariantList chatModels;
    for (QVariant &entry : providers) {
        QVariantMap provider = entry.toMap();
        const QString base = provider.value(QStringLiteral("api_base")).toString();
        const QString current =
            provider.value(QStringLiteral("model")).toString().trimmed();
        const QString custom = providerLabel(base);
        if (provider.value(QStringLiteral("active")).toBool()
            && !custom.isEmpty()) {
            provider.insert(QStringLiteral("name"), custom);
        }
        QVariantList models = providerModels(base);
        QStringList ids;
        for (const QVariant &value : models) {
            const QString id = value.toString().trimmed();
            if (!id.isEmpty() && !ids.contains(id))
                ids.append(id);
        }
        if (!current.isEmpty() && !ids.contains(current))
            ids.prepend(current);
        const bool openai_compatible =
            provider.value(QStringLiteral("protocol")).toString()
            != QStringLiteral("anthropic");
        QVariantList visibleModels;
        for (const QString &id : ids) {
            visibleModels.append(id);
            if (!openai_compatible)
                continue;
            QVariantMap item;
            item.insert(QStringLiteral("model"), id);
            item.insert(QStringLiteral("provider"),
                        provider.value(QStringLiteral("name")));
            item.insert(QStringLiteral("apiBase"), base);
            item.insert(QStringLiteral("protocol"), QStringLiteral("openai"));
            item.insert(QStringLiteral("active"),
                        provider.value(QStringLiteral("active")).toBool()
                            && id == current);
            item.insert(QStringLiteral("hasKey"),
                        provider.value(QStringLiteral("has_key")));
            chatModels.append(item);
        }
        provider.insert(QStringLiteral("models"), visibleModels);
        entry = provider;
    }
    const bool providers_differ = providers != m_providers;
    const bool models_differ = chatModels != m_chat_models;
    m_providers = providers;
    m_chat_models = chatModels;
    if (providers_differ)
        emit providersChanged();
    if (models_differ)
        emit chatModelsChanged();
}

void AppController::setProviderModels(const QString &apiBase,
                                      const QVariantList &models) {
    setProviderModelsStore(apiBase, models);
    refreshProviders();
}

QVariantMap AppController::activateModel(const QString &apiBase,
                                         const QString &providerName,
                                         const QString &model) {
    if (model.trimmed().isEmpty())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("模型名不能为空")}};
    for (const QVariant &entry : m_providers) {
        const QVariantMap provider = entry.toMap();
        if (provider.value(QStringLiteral("api_base")).toString() != apiBase)
            continue;
        if (provider.value(QStringLiteral("protocol")).toString()
            == QStringLiteral("anthropic")) {
            return {{QStringLiteral("ok"), false},
                    {QStringLiteral("error"),
                     QStringLiteral("第三方接入仅支持 OpenAI 兼容协议")}};
        }
        break;
    }
    const QVariantMap result = saveProvider(providerName, apiBase, QString(),
                                            model.trimmed(),
                                            QStringLiteral("openai"), true);
    if (result.value(QStringLiteral("ok"), false).toBool()) {
        refreshProviders();
        emit toastRequested(QStringLiteral("已切换到 %1").arg(model.trimmed()));
    }
    return result;
}

QVariantMap AppController::saveProvider(const QString &name,
                                        const QString &apiBase,
                                        const QString &apiKey,
                                        const QString &model,
                                        const QString &protocol,
                                        bool activate) {
    if (!requireIdle(QStringLiteral("切换供应商")))
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("session is generating")}};
    QString currentName;
    for (const QVariant &entry : m_providers) {
        const QVariantMap provider = entry.toMap();
        if (provider.value(QStringLiteral("active")).toBool()) {
            currentName = provider.value(QStringLiteral("name")).toString();
            break;
        }
    }
    const QVariantMap result =
        m_bridge.saveProvider(name, apiBase, apiKey, model, protocol, activate,
                              currentName);
    if (result.value(QStringLiteral("ok"), false).toBool()) {
        setProviderLabel(apiBase, name);
        refreshProviders();
        refreshSessionApprovals();
    } else {
        const QString error = result.value(QStringLiteral("error")).toString();
        if (!error.isEmpty())
            emit toastRequested(error);
    }
    return result;
}

void AppController::deleteProvider(const QString &name) {
    const QVariantMap result = m_bridge.deleteProvider(name);
    if (!result.value(QStringLiteral("ok"), false).toBool()) {
        emit toastRequested(result.value(QStringLiteral("error")).toString());
        return;
    }
    refreshProviders();
    emit toastRequested(QStringLiteral("供应商已删除"));
}

void AppController::fetchModels(const QString &apiBase,
                                const QString &apiKey,
                                const QString &protocol) {
    if (m_model_fetch_busy)
        return;
    const QVariantMap result = m_bridge.startModelFetch(apiBase, apiKey, protocol);
    if (!result.value(QStringLiteral("ok"), false).toBool()) {
        m_model_fetch_result = result;
        emit modelFetchResultChanged();
        emit modelFetchFinished(result);
        return;
    }
    m_model_fetch_busy = true;
    m_model_fetch_result = {{QStringLiteral("base"), apiBase},
                            {QStringLiteral("protocol"), protocol}};
    emit modelFetchBusyChanged();
    emit modelFetchResultChanged();
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
    refreshSessionApprovals();
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
    refreshSessionApprovals();
}

bool AppController::requireIdle(const QString &action) {
    if (!generating())
        return true;
    emit toastRequested(QStringLiteral("请先停止当前回答，再%1").arg(action));
    return false;
}

QVariantMap AppController::savePolicy(const QVariantMap &fields) {
    if (!requireIdle(QStringLiteral("修改安全策略")))
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("session is generating")}};
    const QVariantMap result = m_bridge.savePolicy(fields);
    if (result.value(QStringLiteral("ok"), false).toBool()) {
        m_policy_info = result;
        emit policyInfoChanged();
        refreshSessionApprovals();
        emit toastRequested(QStringLiteral("aish 安全策略已保存"));
    } else {
        const QString error = result.value(QStringLiteral("error")).toString();
        emit toastRequested(error.isEmpty() ? QStringLiteral("安全策略保存失败")
                                            : error);
    }
    return result;
}

QVariantMap AppController::savePolicyRules(const QVariantList &rules) {
    if (!requireIdle(QStringLiteral("修改安全规则")))
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("session is generating")}};
    const QString fingerprint =
        m_policy_info.value(QStringLiteral("rule_file_fingerprint")).toString();
    const QVariantMap result = m_bridge.savePolicyRules(rules, fingerprint);
    if (result.value(QStringLiteral("ok"), false).toBool()) {
        m_policy_info = result;
        emit policyInfoChanged();
        refreshSessionApprovals();
        emit toastRequested(QStringLiteral("aish 安全规则已保存"));
    } else {
        const QString error = result.value(QStringLiteral("error")).toString();
        emit toastRequested(error.isEmpty() ? QStringLiteral("安全规则保存失败")
                                            : error);
    }
    return result;
}

QVariantMap AppController::previewPolicy(const QString &command,
                                         const QVariantList &draftRules) {
    return m_bridge.previewPolicyCommand(command, draftRules);
}

void AppController::refreshSessionApprovals() {
    if (sessionId().isEmpty()) {
        if (!m_session_approvals.isEmpty()) {
            m_session_approvals.clear();
            emit sessionApprovalsChanged();
        }
        return;
    }
    const QVariantList items = m_bridge.sessionApprovals(sessionId());
    if (items == m_session_approvals)
        return;
    m_session_approvals = items;
    emit sessionApprovalsChanged();
}

void AppController::revokeSessionApproval(const QString &kind,
                                          const QString &first,
                                          const QString &second) {
    if (sessionId().isEmpty())
        return;
    const QVariantMap result =
        m_bridge.revokeSessionApproval(sessionId(), kind, first, second);
    if (!result.value(QStringLiteral("ok"), false).toBool())
        emit toastRequested(result.value(QStringLiteral("error")).toString());
    refreshSessionApprovals();
}

void AppController::clearSessionApprovals() {
    if (sessionId().isEmpty())
        return;
    const QVariantMap result = m_bridge.clearSessionApprovals(sessionId());
    if (!result.value(QStringLiteral("ok"), false).toBool())
        emit toastRequested(result.value(QStringLiteral("error")).toString());
    refreshSessionApprovals();
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

void AppController::searchSkills(const QString &query) {
    if (m_skill_search_busy)
        return;
    const QVariantMap result = m_bridge.startSkillSearch(query, 20);
    if (!result.value(QStringLiteral("ok"), false).toBool()) {
        emit toastRequested(result.value(QStringLiteral("error")).toString());
        return;
    }
    m_skill_search_busy = true;
    m_skill_search_result = {{QStringLiteral("query"), query}};
    emit skillSearchBusyChanged();
    emit skillSearchResultChanged();
}

void AppController::installSkill(const QString &skillId) {
    if (!requireIdle(QStringLiteral("安装技能")))
        return;

    if (m_skill_install_busy)
        return;
    const QVariantMap result = m_bridge.startSkillInstall(skillId);
    if (!result.value(QStringLiteral("ok"), false).toBool()) {
        emit toastRequested(result.value(QStringLiteral("error")).toString());
        return;
    }
    m_skill_install_busy = true;
    emit skillInstallBusyChanged();
    emit toastRequested(QStringLiteral("正在安装技能，完成后会进入隔离审查"));
}

void AppController::reviewSkill(const QString &name) {
    QVariantMap review = m_bridge.readSkill(name, 256 * 1024);
    const QVariantMap verify = m_bridge.verifySkill(name);
    review.insert(QStringLiteral("verify"), verify);
    m_skill_review = review;
    emit skillReviewChanged();
    emit skillReviewFinished(review);
}

void AppController::trustSkill(const QString &name) {
    if (!requireIdle(QStringLiteral("信任技能")))
        return;

    const QVariantMap result = m_bridge.trustSkill(name);
    if (result.value(QStringLiteral("ok"), false).toBool()) {
        emit toastRequested(result.value(QStringLiteral("message")).toString());
        refreshSkillCatalog();
        m_bridge.refreshSkillsLive();
        refreshToolCatalog();
        reviewSkill(name);
    } else {
        emit toastRequested(result.value(QStringLiteral("error")).toString());
    }
}

void AppController::removeSkill(const QString &name) {
    if (!requireIdle(QStringLiteral("删除技能")))
        return;

    const QVariantMap result = m_bridge.removeSkill(name);
    if (result.value(QStringLiteral("ok"), false).toBool()) {
        emit toastRequested(QStringLiteral("技能已删除"));
        refreshSkillCatalog();
        refreshToolCatalog();
        if (m_skill_review.value(QStringLiteral("name")).toString() == name) {
            m_skill_review.clear();
            emit skillReviewChanged();
        }
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
    if (!requireIdle(QStringLiteral("重载 MCP")))
        return;

    const QVariantMap result = m_bridge.reloadMcp();
    if (!result.value(QStringLiteral("ok"), false).toBool()) {
        emit toastRequested(result.value(QStringLiteral("error")).toString());
        return;
    }
    m_mcp_busy = true;
    emit mcpBusyChanged();
    emit toastRequested(QStringLiteral("正在重载 MCP 连接"));
}

void AppController::addMcpServer(const QVariantMap &args) {
    if (!requireIdle(QStringLiteral("添加 MCP 服务器")))
        return;

    const QVariantMap result = m_bridge.addMcpServer(args);
    if (!result.value(QStringLiteral("ok"), false).toBool()) {
        emit toastRequested(result.value(QStringLiteral("error")).toString());
        return;
    }
    m_mcp_busy = true;
    emit mcpBusyChanged();
    refreshMcpStatus();
    emit toastRequested(QStringLiteral("MCP server 已添加，正在连接"));
}

void AppController::updateMcpServer(const QString &name,
                                    const QVariantMap &args) {
    if (!requireIdle(QStringLiteral("修改 MCP 服务器")))
        return;
    const QVariantMap result = m_bridge.updateMcpServer(name, args);
    if (!result.value(QStringLiteral("ok"), false).toBool()) {
        emit toastRequested(result.value(QStringLiteral("error")).toString());
        return;
    }
    m_mcp_busy = true;
    emit mcpBusyChanged();
    refreshMcpStatus();
}

void AppController::removeMcpServer(const QString &name) {
    if (!requireIdle(QStringLiteral("删除 MCP 服务器")))
        return;

    const QVariantMap result = m_bridge.removeMcpServer(name);
    if (!result.value(QStringLiteral("ok"), false).toBool()) {
        emit toastRequested(result.value(QStringLiteral("error")).toString());
        return;
    }
    m_mcp_busy = true;
    emit mcpBusyChanged();
    emit toastRequested(QStringLiteral("MCP server 已移除"));
}

void AppController::setMcpMasterEnabled(bool enabled) {
    if (!requireIdle(QStringLiteral("修改 MCP 总开关")))
        return;

    const QVariantMap result = m_bridge.setMcpMasterEnabled(enabled);
    if (!result.value(QStringLiteral("ok"), false).toBool()) {
        emit toastRequested(result.value(QStringLiteral("error")).toString());
        return;
    }
    m_mcp_busy = true;
    emit mcpBusyChanged();
    refreshMcpStatus();
}

void AppController::setMcpEnabled(const QString &name, bool enabled) {
    if (!requireIdle(QStringLiteral("启停 MCP 服务器")))
        return;

    const QVariantMap result = m_bridge.setMcpEnabled(name, enabled);
    if (!result.value(QStringLiteral("ok"), false).toBool()) {
        emit toastRequested(result.value(QStringLiteral("error")).toString());
        return;
    }
    m_mcp_busy = true;
    emit mcpBusyChanged();
    refreshMcpStatus();
}

void AppController::setMcpTrust(const QString &name,
                                const QString &tool,
                                const QString &level) {
    if (!requireIdle(QStringLiteral("修改 MCP 信任")))
        return;
    const QVariantMap result = m_bridge.setMcpTrust(name, tool, level);
    if (!result.value(QStringLiteral("ok"), false).toBool()) {
        emit toastRequested(result.value(QStringLiteral("error")).toString());
        return;
    }
    m_mcp_busy = true;
    emit mcpBusyChanged();
    refreshMcpStatus();
}

void AppController::testMcpServer(const QString &name) {
    const QVariantMap result = m_bridge.testMcpServer(name);
    if (!result.value(QStringLiteral("ok"), false).toBool()) {
        emit toastRequested(result.value(QStringLiteral("error")).toString());
        return;
    }
    m_mcp_test_result = {{QStringLiteral("job"), QStringLiteral("mcp_test")},
                         {QStringLiteral("pending"), true}};
    m_mcp_busy = true;
    emit mcpBusyChanged();
    emit mcpTestResultChanged();
}

void AppController::testMcpDraft(const QVariantMap &args) {
    const QVariantMap result = m_bridge.testMcpDraft(args);
    if (!result.value(QStringLiteral("ok"), false).toBool()) {
        emit toastRequested(result.value(QStringLiteral("error")).toString());
        return;
    }
    m_mcp_test_result = {{QStringLiteral("job"), QStringLiteral("mcp_test")},
                         {QStringLiteral("pending"), true}};
    m_mcp_busy = true;
    emit mcpBusyChanged();
    emit mcpTestResultChanged();
}

void AppController::onSettingsJobFinished(const QVariantMap &result) {
    const QString job = result.value(QStringLiteral("job")).toString();
    const bool ok = result.value(QStringLiteral("ok"), false).toBool();
    if (job == QStringLiteral("skill_search")) {
        m_skill_search_busy = false;
        emit skillSearchBusyChanged();
        m_skill_search_result = result.value(QStringLiteral("data")).toMap();
        m_skill_search_result.insert(QStringLiteral("ok"), ok);
        m_skill_search_result.insert(QStringLiteral("error"),
                                     result.value(QStringLiteral("error")));
        emit skillSearchResultChanged();
        emit skillSearchFinished(m_skill_search_result);
    } else if (job == QStringLiteral("skill_install")) {
        m_skill_install_busy = false;
        emit skillInstallBusyChanged();
        QVariantMap adjusted = result;
        if (ok) {
            m_bridge.refreshSkillsLive();
            refreshSkillCatalog();
            refreshToolCatalog();
            // The install job identifies the skill by its directory slug (the
            // registry slug), but SKILL.md can declare a different name.
            // Resolve the loaded catalog name so read/verify/trust all target
            // the same record.
            QVariantMap data = adjusted.value(QStringLiteral("data")).toMap();
            QVariantMap meta = data.value(QStringLiteral("meta")).toMap();
            const QString installedPath = meta.value(QStringLiteral("path")).toString();
            QString displayName;
            const QVariantList skills =
                m_skill_catalog.value(QStringLiteral("skills")).toList();
            for (const QVariant &entry : skills) {
                const QVariantMap skill = entry.toMap();
                if (skill.value(QStringLiteral("base_dir")).toString() == installedPath) {
                    displayName = skill.value(QStringLiteral("name")).toString();
                    break;
                }
            }
            if (!displayName.isEmpty())
                meta.insert(QStringLiteral("display_name"), displayName);
            data.insert(QStringLiteral("meta"), meta);
            adjusted.insert(QStringLiteral("data"), data);
            emit toastRequested(
                QStringLiteral("技能已安装并处于隔离状态，请审查后信任"));
        } else {
            emit toastRequested(result.value(QStringLiteral("error")).toString());
        }
        m_skill_install_result = adjusted;
        emit skillInstallResultChanged();
        emit skillInstallFinished(m_skill_install_result);
    } else if (job == QStringLiteral("model_fetch")) {
        m_model_fetch_busy = false;
        emit modelFetchBusyChanged();
        m_model_fetch_result = result;
        emit modelFetchResultChanged();
        emit modelFetchFinished(result);
        if (!ok)
            emit toastRequested(result.value(QStringLiteral("error")).toString());
    } else if (job == QStringLiteral("mcp_apply")
               || job == QStringLiteral("mcp_reload")) {
        m_mcp_busy = false;
        emit mcpBusyChanged();
        m_bridge.refreshMcpTools();
        refreshMcpStatus();
        refreshToolCatalog();
        emit toastRequested(job == QStringLiteral("mcp_reload")
                                ? QStringLiteral("MCP 连接已重载")
                                : QStringLiteral("MCP 配置已应用"));
    } else if (job == QStringLiteral("mcp_test")) {
        m_mcp_busy = false;
        emit mcpBusyChanged();
        m_mcp_test_result = result;
        emit mcpTestResultChanged();
        emit mcpTestFinished(result);
        if (!ok)
            emit toastRequested(result.value(QStringLiteral("error")).toString());
    }
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
                                    const QString &model, int total, int start) {
    m_history_total = total;
    m_history_start = start;
    m_history_loading = false;
    m_chat.loadTranscript(messages);
    m_assistant_open = false;
    m_model = model.isEmpty() ? m_bridge.model() : model;
    emit modelChanged();
    m_local_title.clear();
    recomputeTitle();
    refreshSessionApprovals();
    emit historyWindowChanged();
    emit sessionLoaded();
}

void AppController::loadOlderMessages() {
    if (m_history_loading || m_history_start <= 0 || sessionId().isEmpty())
        return;
    m_history_loading = true;
    emit historyWindowChanged();
    const QVariantMap result =
        m_bridge.historyPage(sessionId(), m_history_start, 40);
    m_history_loading = false;
    if (!result.value(QStringLiteral("ok"), false).toBool()) {
        emit historyWindowChanged();
        emit toastRequested(result.value(QStringLiteral("error")).toString());
        return;
    }
    const QVariantList messages =
        result.value(QStringLiteral("messages")).toList();
    m_chat.prependTranscript(messages);
    m_history_start = result.value(QStringLiteral("start"), m_history_start).toInt();
    m_history_total = result.value(QStringLiteral("total"), m_history_total).toInt();
    emit historyWindowChanged();
    emit historyOlderLoaded(messages.size());
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
    m_history_total = 0;
    m_history_start = 0;
    m_history_loading = false;
    recomputeTitle();
    refreshSessionApprovals();
    emit historyWindowChanged();
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
