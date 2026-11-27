#include "aishbridge.h"

#include <QClipboard>
#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPixmap>
#include <QScreen>
#include <QStandardPaths>
#include <QUrl>

namespace {

QString toQString(const rust::cxxbridge1::String &value) {
    return QString::fromStdString(static_cast<std::string>(value));
}

bool parseResponse(const QString &raw, QJsonObject *object, QString *error) {
    if (raw.isEmpty()) {
        *error = QStringLiteral("AI 服务返回了空响应");
        return false;
    }
    QJsonParseError parse_error;
    const QJsonDocument document =
        QJsonDocument::fromJson(raw.toUtf8(), &parse_error);
    if (parse_error.error != QJsonParseError::NoError || !document.isObject()) {
        *error = QStringLiteral("AI 服务返回了无效数据");
        return false;
    }
    *object = document.object();
    if (!object->value(QStringLiteral("ok")).toBool(false)) {
        *error = object->value(QStringLiteral("error"))
                     .toString(QStringLiteral("AI 服务操作失败"));
        return false;
    }
    return true;
}

QVariantMap parseObject(const QString &raw, QString *error) {
    QJsonObject object;
    if (!parseResponse(raw, &object, error))
        return {};
    return object.toVariantMap();
}

bool isAllowedImage(const QString &path) {
    static const QStringList extensions = {
        QStringLiteral("png"),  QStringLiteral("jpg"),
        QStringLiteral("jpeg"), QStringLiteral("webp"),
        QStringLiteral("gif"),  QStringLiteral("bmp"),
    };
    return extensions.contains(QFileInfo(path).suffix().toLower());
}

} // namespace

AishBridge::AishBridge(QObject *parent)
    : QObject(parent),
      m_workspace(QDir::currentPath()),
      m_default_workspace(QDir::currentPath()) {
    qRegisterMetaType<QVariantMap>("QVariantMap");

    m_poll_timer.setInterval(20);
    m_poll_timer.setTimerType(Qt::CoarseTimer);
    connect(&m_poll_timer, &QTimer::timeout, this, &AishBridge::pollEvents);

    m_flush_timer.setSingleShot(true);
    m_flush_timer.setInterval(33);
    m_flush_timer.setTimerType(Qt::CoarseTimer);
    connect(&m_flush_timer, &QTimer::timeout, this, &AishBridge::flushPending);

    cleanupOldScreenshots();
}

AishBridge::~AishBridge() {
    m_poll_timer.stop();
    m_flush_timer.stop();
    if (m_service) {
        shutdown(**m_service);
        delete m_service;
        m_service = nullptr;
    }
}

bool AishBridge::ready() const { return m_ready; }
bool AishBridge::generating() const { return m_generating; }
QString AishBridge::lastError() const { return m_last_error; }
QString AishBridge::sessionId() const { return m_session_id; }

QString AishBridge::model() const {
    return m_config.value(QStringLiteral("model")).toString();
}

QString AishBridge::apiBase() const {
    return m_config.value(QStringLiteral("api_base")).toString();
}

bool AishBridge::hasApiKey() const {
    return m_config.value(QStringLiteral("has_api_key")).toBool();
}

QString AishBridge::configPath() const {
    return m_config.value(QStringLiteral("config_path")).toString();
}

QVariantMap AishBridge::config() const { return m_config; }
QVariantList AishBridge::sessions() const { return m_sessions; }
QString AishBridge::workspace() const { return m_workspace; }
QString AishBridge::defaultWorkspace() const { return m_default_workspace; }

QString AishBridge::appVersion() const {
    return QCoreApplication::applicationVersion();
}

BridgeService *AishBridge::service() const {
    if (!m_service)
        return nullptr;
    return &(**m_service);
}

void AishBridge::initialize() {
    if (m_ready)
        return;
    if (m_service) {
        shutdown(**m_service);
        delete m_service;
        m_service = nullptr;
    }
    m_service =
        new rust::Box<BridgeService>(new_service_from_default_config());
    refreshConfig();
    if (!m_ready)
        return;
    refreshSessions();
    startPolling();
}

bool AishBridge::ensureService() {
    if (!m_service)
        initialize();
    return m_service && m_ready;
}

void AishBridge::startPolling() {
    if (!m_poll_timer.isActive())
        m_poll_timer.start();
}

void AishBridge::setReady(bool ready) {
    if (m_ready == ready)
        return;
    m_ready = ready;
    emit readyChanged();
}

void AishBridge::setGenerating(bool generating) {
    if (m_generating == generating)
        return;
    m_generating = generating;
    emit generatingChanged();
}

void AishBridge::setLastError(const QString &message) {
    const QString trimmed = message.trimmed();
    if (m_last_error == trimmed)
        return;
    m_last_error = trimmed;
    emit lastErrorChanged();
    if (!trimmed.isEmpty())
        emit errorOccurred(trimmed);
}

void AishBridge::applyConfig(const QJsonObject &config) {
    const QVariantMap next = config.toVariantMap();
    if (next == m_config)
        return;
    m_config = next;
    emit configChanged();
}

void AishBridge::refreshConfig() {
    if (!m_service) {
        setReady(false);
        return;
    }
    QString raw = toQString(config_info(**m_service));
    QJsonObject object;
    QString error;
    if (!parseResponse(raw, &object, &error)) {
        setLastError(error);
        setReady(false);
        return;
    }
    applyConfig(object.value(QStringLiteral("config")).toObject());
    setLastError(QString());
    setReady(true);
}

bool AishBridge::createSession(const QString &model) {
    if (!ensureService()) {
        setLastError(QStringLiteral("AI 服务尚未就绪"));
        return false;
    }
    if (m_generating) {
        setLastError(QStringLiteral("请等待当前回答结束"));
        return false;
    }
    QString chosen = model.trimmed();
    if (chosen.isEmpty())
        chosen = this->model();
    if (chosen.isEmpty()) {
        setLastError(QStringLiteral("尚未配置模型"));
        return false;
    }

    const std::string chosen_std = chosen.toStdString();
    QString raw = toQString(create_session(**m_service, chosen_std));
    QJsonObject object;
    QString error;
    if (!parseResponse(raw, &object, &error)) {
        setLastError(error);
        return false;
    }
    const QString id = object.value(QStringLiteral("session_id")).toString();
    if (id.isEmpty()) {
        setLastError(QStringLiteral("创建会话失败：缺少会话 ID"));
        return false;
    }
    m_session_id = id;
    m_turn_started_at = 0;
    emit sessionChanged();
    return true;
}

bool AishBridge::sendMessage(const QString &text,
                             const QVariantList &imagePaths,
                             const QString &model) {
    if (!ensureService()) {
        setLastError(QStringLiteral("AI 服务尚未就绪"));
        return false;
    }
    if (m_generating) {
        setLastError(QStringLiteral("请等待当前回答结束"));
        return false;
    }
    if (text.trimmed().isEmpty() && imagePaths.isEmpty())
        return false;
    if (imagePaths.size() > 4) {
        setLastError(QStringLiteral("每条消息最多附加 4 张图片"));
        return false;
    }

    QJsonArray images;
    for (const QVariant &entry : imagePaths) {
        const QString raw_path = entry.toString();
        const QFileInfo info(raw_path);
        const QString canonical = info.canonicalFilePath();
        if (canonical.isEmpty() || !QFileInfo(canonical).isFile() ||
            !isAllowedImage(canonical)) {
            setLastError(QStringLiteral("无法读取图片：%1").arg(raw_path));
            return false;
        }
        if (QFileInfo(canonical).size() > 10 * 1024 * 1024) {
            setLastError(QStringLiteral("图片超过 10 MB：%1").arg(info.fileName()));
            return false;
        }
        images.append(canonical);
    }

    if (m_session_id.isEmpty()) {
        if (!createSession(model))
            return false;
    }

    const std::string session_std = m_session_id.toStdString();
    const std::string text_std = text.toStdString();
    const std::string images_std =
        QString::fromUtf8(
            QJsonDocument(images).toJson(QJsonDocument::Compact))
            .toStdString();
    QString raw = toQString(
        send_message(**m_service, session_std, text_std, images_std));
    QJsonObject object;
    QString error;
    if (!parseResponse(raw, &object, &error)) {
        setLastError(error);
        return false;
    }
    m_turn_started_at = QDateTime::currentMSecsSinceEpoch();
    m_pending_content.clear();
    m_pending_reasoning.clear();
    setGenerating(true);
    setLastError(QString());
    return true;
}

void AishBridge::cancel() {
    if (!m_service || m_session_id.isEmpty())
        return;
    cancel_session(**m_service, m_session_id.toStdString());
}

void AishBridge::approve(const QString &requestId, int decision) {
    if (!m_service) {
        emit toastRequested(QStringLiteral("审批请求已失效"));
        return;
    }
    const bool ok =
        ::approve(**m_service, requestId.toStdString(), decision);
    if (!ok)
        emit toastRequested(QStringLiteral("审批请求已失效"));
}

bool AishBridge::respondInteraction(const QString &requestId,
                                    const QVariantMap &response) {
    if (!m_service || requestId.isEmpty())
        return false;
    const QByteArray json =
        QJsonDocument(QJsonObject::fromVariantMap(response))
            .toJson(QJsonDocument::Compact);
    return ::respond_interaction(**m_service, requestId.toStdString(),
                                 json.toStdString());
}

QVariantMap AishBridge::policyInfo() {
    if (!ensureService())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), lastError()}};
    QString raw = toQString(policy_info(**m_service));
    QJsonObject object;
    QString error;
    if (!parseResponse(raw, &object, &error)) {
        setLastError(error);
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), error}};
    }
    return object.toVariantMap();
}

QVariantMap AishBridge::savePolicy(const QVariantMap &fields) {
    if (!ensureService())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), lastError()}};
    const QByteArray json =
        QJsonDocument(QJsonObject::fromVariantMap(fields))
            .toJson(QJsonDocument::Compact);
    QString raw = toQString(save_policy(**m_service, json.toStdString()));
    QJsonObject object;
    QString error;
    if (!parseResponse(raw, &object, &error)) {
        setLastError(error);
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), error}};
    }
    return object.toVariantMap();
}

QVariantMap AishBridge::savePolicyRules(const QVariantList &rules,
                                        const QString &fingerprint) {
    if (!ensureService())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), lastError()}};
    const QByteArray json =
        QJsonDocument(QJsonArray::fromVariantList(rules))
            .toJson(QJsonDocument::Compact);
    QString error;
    const QVariantMap result = parseObject(
        toQString(save_policy_rules(**m_service, json.toStdString(),
                                    fingerprint.toStdString())),
        &error);
    if (!error.isEmpty()) {
        setLastError(error);
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), error}};
    }
    return result;
}

QVariantMap AishBridge::previewPolicyCommand(const QString &command,
                                             const QVariantList &draftRules) {
    if (!ensureService())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), lastError()}};
    const QByteArray json =
        QJsonDocument(QJsonArray::fromVariantList(draftRules))
            .toJson(QJsonDocument::Compact);
    QString error;
    const QVariantMap result = parseObject(
        toQString(preview_policy_command(**m_service, command.toStdString(),
                                         json.toStdString())),
        &error);
    if (!error.isEmpty()) {
        setLastError(error);
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), error}};
    }
    return result;
}

QVariantList AishBridge::sessionApprovals(const QString &sessionId) {
    if (!ensureService() || sessionId.isEmpty())
        return {};
    QString error;
    const QVariantMap result = parseObject(
        toQString(session_approvals(**m_service, sessionId.toStdString())),
        &error);
    if (!error.isEmpty()) {
        setLastError(error);
        return {};
    }
    return result.value(QStringLiteral("items")).toList();
}

QVariantMap AishBridge::revokeSessionApproval(const QString &sessionId,
                                              const QString &kind,
                                              const QString &first,
                                              const QString &second) {
    if (!ensureService())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), lastError()}};
    QString error;
    const QVariantMap result = parseObject(
        toQString(revoke_session_approval(**m_service, sessionId.toStdString(),
                                          kind.toStdString(), first.toStdString(),
                                          second.toStdString())),
        &error);
    if (!error.isEmpty()) {
        setLastError(error);
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), error}};
    }
    return result;
}

QVariantMap AishBridge::clearSessionApprovals(const QString &sessionId) {
    if (!ensureService())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), lastError()}};
    QString error;
    const QVariantMap result = parseObject(
        toQString(clear_session_approvals(**m_service, sessionId.toStdString())),
        &error);
    if (!error.isEmpty()) {
        setLastError(error);
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), error}};
    }
    return result;
}

QVariantList AishBridge::toolCatalog() {
    if (!ensureService())
        return {};
    QString raw = toQString(tool_catalog(**m_service));
    QJsonObject object;
    QString error;
    if (!parseResponse(raw, &object, &error)) {
        setLastError(error);
        return {};
    }
    return object.value(QStringLiteral("tools")).toArray().toVariantList();
}

QVariantMap AishBridge::skillCatalog() {
    if (!ensureService())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), lastError()}};
    QString raw = toQString(skill_catalog(**m_service));
    QJsonObject object;
    QString error;
    if (!parseResponse(raw, &object, &error)) {
        setLastError(error);
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), error}};
    }
    return object.toVariantMap();
}

QVariantMap AishBridge::trustSkill(const QString &name) {
    if (!ensureService())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), lastError()}};
    QString raw = toQString(trust_skill(**m_service, name.toStdString()));
    QJsonObject object;
    QString error;
    if (!parseResponse(raw, &object, &error)) {
        setLastError(error);
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), error}};
    }
    return object.toVariantMap();
}

QVariantMap AishBridge::refreshSkillsLive() {
    if (!ensureService())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), lastError()}};
    QString error;
    const QVariantMap result =
        parseObject(toQString(refresh_skills_live(**m_service)), &error);
    if (!error.isEmpty()) {
        setLastError(error);
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), error}};
    }
    return result;
}

QVariantMap AishBridge::readSkill(const QString &name, int maxBytes) {
    if (!ensureService())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), lastError()}};
    QString error;
    const QVariantMap result = parseObject(
        toQString(skill_read(**m_service, name.toStdString(),
                             static_cast<uint32_t>(qMax(1, maxBytes)))),
        &error);
    if (!error.isEmpty()) {
        setLastError(error);
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), error}};
    }
    return result;
}

QVariantMap AishBridge::verifySkill(const QString &name) {
    if (!ensureService())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), lastError()}};
    QString error;
    const QVariantMap result =
        parseObject(toQString(skill_verify(**m_service, name.toStdString())),
                    &error);
    if (!error.isEmpty()) {
        setLastError(error);
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), error}};
    }
    return result;
}

QVariantMap AishBridge::removeSkill(const QString &name) {
    if (!ensureService())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), lastError()}};
    QString error;
    const QVariantMap result =
        parseObject(toQString(skill_remove(**m_service, name.toStdString())),
                    &error);
    if (!error.isEmpty()) {
        setLastError(error);
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), error}};
    }
    return result;
}

QVariantMap AishBridge::startSkillSearch(const QString &query, int limit) {
    if (!ensureService())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), lastError()}};
    QString error;
    const QVariantMap result = parseObject(
        toQString(skill_search_start(**m_service, query.toStdString(),
                                     static_cast<uint32_t>(qBound(1, limit, 50)))),
        &error);
    if (!error.isEmpty()) {
        setLastError(error);
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), error}};
    }
    return result;
}

QVariantMap AishBridge::startSkillInstall(const QString &skillId) {
    if (!ensureService())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), lastError()}};
    QString error;
    const QVariantMap result = parseObject(
        toQString(skill_install_start(**m_service, skillId.toStdString())),
        &error);
    if (!error.isEmpty()) {
        setLastError(error);
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), error}};
    }
    return result;
}

QVariantMap AishBridge::mcpStatus() {
    if (!ensureService())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), lastError()}};
    QString raw = toQString(mcp_status(**m_service));
    QJsonObject object;
    QString error;
    if (!parseResponse(raw, &object, &error)) {
        setLastError(error);
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), error}};
    }
    return object.toVariantMap();
}

QVariantMap AishBridge::reloadMcp() {
    if (!ensureService())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), lastError()}};
    QString raw = toQString(mcp_reload(**m_service));
    QJsonObject object;
    QString error;
    if (!parseResponse(raw, &object, &error)) {
        setLastError(error);
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), error}};
    }
    return object.toVariantMap();
}

QVariantMap AishBridge::refreshMcpTools() {
    if (!ensureService())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), lastError()}};
    QString error;
    const QVariantMap result =
        parseObject(toQString(refresh_mcp_tools(**m_service)), &error);
    if (!error.isEmpty()) {
        setLastError(error);
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), error}};
    }
    return result;
}

QVariantMap AishBridge::addMcpServer(const QVariantMap &args) {
    if (!ensureService())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), lastError()}};
    const QByteArray json =
        QJsonDocument(QJsonObject::fromVariantMap(args))
            .toJson(QJsonDocument::Compact);
    QString error;
    const QVariantMap result = parseObject(
        toQString(mcp_add(**m_service, json.toStdString())), &error);
    if (!error.isEmpty()) {
        setLastError(error);
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), error}};
    }
    return result;
}

QVariantMap AishBridge::updateMcpServer(const QString &name,
                                        const QVariantMap &args) {
    if (!ensureService())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), lastError()}};
    const QByteArray json =
        QJsonDocument(QJsonObject::fromVariantMap(args))
            .toJson(QJsonDocument::Compact);
    QString error;
    const QVariantMap result = parseObject(
        toQString(mcp_update(**m_service, name.toStdString(), json.toStdString())),
        &error);
    if (!error.isEmpty()) {
        setLastError(error);
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), error}};
    }
    return result;
}

QVariantMap AishBridge::removeMcpServer(const QString &name) {
    if (!ensureService())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), lastError()}};
    QString error;
    const QVariantMap result =
        parseObject(toQString(mcp_remove(**m_service, name.toStdString())), &error);
    if (!error.isEmpty()) {
        setLastError(error);
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), error}};
    }
    return result;
}

QVariantMap AishBridge::setMcpMasterEnabled(bool enabled) {
    if (!ensureService())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), lastError()}};
    QString error;
    const QVariantMap result =
        parseObject(toQString(mcp_set_master_enabled(**m_service, enabled)), &error);
    if (!error.isEmpty()) {
        setLastError(error);
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), error}};
    }
    return result;
}

QVariantMap AishBridge::setMcpEnabled(const QString &name, bool enabled) {
    if (!ensureService())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), lastError()}};
    QString error;
    const QVariantMap result = parseObject(
        toQString(mcp_set_enabled(**m_service, name.toStdString(), enabled)),
        &error);
    if (!error.isEmpty()) {
        setLastError(error);
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), error}};
    }
    return result;
}

QVariantMap AishBridge::setMcpTrust(const QString &name,
                                    const QString &tool,
                                    const QString &level) {
    if (!ensureService())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), lastError()}};
    QString error;
    const QVariantMap result = parseObject(
        toQString(mcp_set_trust(**m_service, name.toStdString(),
                                tool.toStdString(), level.toStdString())),
        &error);
    if (!error.isEmpty()) {
        setLastError(error);
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), error}};
    }
    return result;
}

QVariantMap AishBridge::testMcpServer(const QString &name) {
    if (!ensureService())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), lastError()}};
    QString error;
    const QVariantMap result =
        parseObject(toQString(mcp_test(**m_service, name.toStdString())), &error);
    if (!error.isEmpty()) {
        setLastError(error);
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), error}};
    }
    return result;
}

QVariantMap AishBridge::testMcpDraft(const QVariantMap &args) {
    if (!ensureService())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), lastError()}};
    const QByteArray json =
        QJsonDocument(QJsonObject::fromVariantMap(args))
            .toJson(QJsonDocument::Compact);
    QString error;
    const QVariantMap result = parseObject(
        toQString(mcp_test_draft(**m_service, json.toStdString())), &error);
    if (!error.isEmpty()) {
        setLastError(error);
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), error}};
    }
    return result;
}

QVariantList AishBridge::memoryList() {
    if (!ensureService())
        return {};
    QString raw = toQString(memory_list(**m_service, 200));
    QJsonObject object;
    QString error;
    if (!parseResponse(raw, &object, &error)) {
        setLastError(error);
        return {};
    }
    return object.value(QStringLiteral("entries")).toArray().toVariantList();
}

QVariantList AishBridge::auditTail() {
    if (!ensureService())
        return {};
    QString raw = toQString(audit_tail(**m_service, 200));
    QJsonObject object;
    QString error;
    if (!parseResponse(raw, &object, &error)) {
        setLastError(error);
        return {};
    }
    return object.value(QStringLiteral("events")).toArray().toVariantList();
}

void AishBridge::refreshSessions() {
    if (!m_service)
        return;
    QString raw = toQString(list_sessions(**m_service, 100));
    QJsonObject object;
    QString error;
    if (!parseResponse(raw, &object, &error))
        return;

    const QVariantList all =
        object.value(QStringLiteral("sessions")).toArray().toVariantList();
    QVariantList visible;
    visible.reserve(all.size());
    for (const QVariant &entry : all) {
        const QVariantMap session = entry.toMap();
        if (session.value(QStringLiteral("summary")).toString().trimmed().isEmpty())
            continue;
        visible.append(session);
    }
    if (visible == m_sessions)
        return;
    m_sessions = visible;
    emit sessionsChanged();
}

void AishBridge::loadSession(const QString &sessionId) {
    if (!ensureService())
        return;
    if (m_generating) {
        setLastError(QStringLiteral("请先停止当前回答"));
        return;
    }
    if (sessionId.trimmed().isEmpty())
        return;

    QString raw = toQString(resume_session_window(**m_service, sessionId.toStdString(), 40));
    QJsonObject object;
    QString error;
    if (!parseResponse(raw, &object, &error)) {
        setLastError(error);
        return;
    }
    const QString model = object.value(QStringLiteral("model")).toString();
    const QVariantList messages =
        object.value(QStringLiteral("messages")).toArray().toVariantList();
    const int total = object.value(QStringLiteral("total")).toInt(messages.size());
    const int start = object.value(QStringLiteral("start")).toInt(0);
    const QString cwd = object.value(QStringLiteral("cwd")).toString();
    const QString warning = object.value(QStringLiteral("warning")).toString();
    m_session_id = sessionId;
    m_turn_started_at = 0;
    setGenerating(false);
    setLastError(QString());
    if (!cwd.isEmpty() && m_workspace != cwd) {
        m_workspace = cwd;
        emit workspaceChanged();
    }
    emit sessionChanged();
    emit sessionLoaded(messages, model, total, start);
    if (!warning.isEmpty())
        emit toastRequested(warning);
}

QVariantMap AishBridge::historyPage(const QString &sessionId,
                                    int before, int limit) {
    if (!ensureService())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), lastError()}};
    QString error;
    const QVariantMap result = parseObject(
        toQString(history_page(**m_service, sessionId.toStdString(),
                               static_cast<uint32_t>(qMax(0, before)),
                               static_cast<uint32_t>(qBound(1, limit, 200)))),
        &error);
    if (!error.isEmpty()) {
        setLastError(error);
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), error}};
    }
    return result;
}

void AishBridge::deleteSession(const QString &sessionId) {
    if (!ensureService())
        return;
    QString raw = toQString(delete_session(**m_service, sessionId.toStdString()));
    QJsonObject object;
    QString error;
    if (!parseResponse(raw, &object, &error)) {
        setLastError(error);
        return;
    }
    for (int i = 0; i < m_sessions.size(); ++i) {
        if (m_sessions.at(i).toMap().value(QStringLiteral("session_id")).toString() ==
            sessionId) {
            m_sessions.removeAt(i);
            break;
        }
    }
    emit sessionsChanged();
    emit sessionDeleted(sessionId);
    if (m_session_id == sessionId) {
        m_session_id.clear();
        setGenerating(false);
        emit sessionChanged();
        emit sessionCleared();
    }
    emit toastRequested(QStringLiteral("对话已删除"));
}

void AishBridge::renameSession(const QString &sessionId,
                               const QString &title) {
    if (!ensureService())
        return;
    QString raw = toQString(
        rename_session(**m_service, sessionId.toStdString(), title.toStdString()));
    QJsonObject object;
    QString error;
    if (!parseResponse(raw, &object, &error)) {
        setLastError(error);
        return;
    }
    for (int i = 0; i < m_sessions.size(); ++i) {
        QVariantMap session = m_sessions.at(i).toMap();
        if (session.value(QStringLiteral("session_id")).toString() == sessionId) {
            session.insert(QStringLiteral("summary"), title);
            m_sessions[i] = session;
            break;
        }
    }
    emit sessionsChanged();
    emit sessionRenamed(sessionId, title);
    emit toastRequested(QStringLiteral("已重命名"));
}

void AishBridge::newConversation() {
    if (m_generating)
        cancel();
    m_session_id.clear();
    m_turn_started_at = 0;
    setGenerating(false);
    setLastError(QString());
    emit sessionChanged();
    emit sessionCleared();
}

void AishBridge::setWorkspace(const QString &path) {
    if (!ensureService()) {
        setLastError(QStringLiteral("AI 服务尚未就绪"));
        return;
    }
    QString raw = toQString(set_workspace(
        **m_service, m_session_id.toStdString(), path.toStdString()));
    QJsonObject object;
    QString error;
    if (!parseResponse(raw, &object, &error)) {
        setLastError(error);
        return;
    }
    const QString canonical =
        object.value(QStringLiteral("path")).toString();
    if (canonical.isEmpty())
        return;
    if (m_workspace == canonical)
        return;
    m_workspace = canonical;
    emit workspaceChanged();
    emit toastRequested(QStringLiteral("工作空间已切换"));
}

void AishBridge::resetWorkspace() {
    setWorkspace(m_default_workspace);
}

void AishBridge::saveModelConfig(const QString &model,
                                 const QString &apiBase,
                                 const QString &apiKey) {
    if (!ensureService())
        return;
    if (m_generating) {
        setLastError(QStringLiteral("请先停止当前回答"));
        return;
    }
    QString raw = toQString(save_model_config(**m_service, model.toStdString(),
                                              apiBase.toStdString(),
                                              apiKey.toStdString()));
    QJsonObject object;
    QString error;
    if (!parseResponse(raw, &object, &error)) {
        setLastError(error);
        return;
    }
    applyConfig(object.value(QStringLiteral("config")).toObject());
    m_session_id.clear();
    setGenerating(false);
    emit sessionChanged();
    emit sessionCleared();
    refreshSessions();
    setLastError(QString());
    emit toastRequested(QStringLiteral("模型配置已保存，新对话将使用该模型"));
}

QVariantMap AishBridge::providersInfo() {
    if (!ensureService())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), lastError()}};
    QString error;
    const QVariantMap result =
        parseObject(toQString(provider_info(**m_service)), &error);
    if (!error.isEmpty()) {
        setLastError(error);
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), error}};
    }
    return result;
}

QVariantMap AishBridge::saveProvider(const QString &name,
                                     const QString &apiBase,
                                     const QString &apiKey,
                                     const QString &model,
                                     const QString &protocol,
                                     bool activate,
                                     const QString &currentName) {
    if (!ensureService())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), lastError()}};
    if (m_generating) {
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), QStringLiteral("请先停止当前回答")}};
    }
    QString error;
    const QVariantMap result = parseObject(
        toQString(save_provider_config(**m_service, name.toStdString(),
                                       apiBase.toStdString(), apiKey.toStdString(),
                                       model.toStdString(), protocol.toStdString(),
                                       activate, currentName.toStdString())),
        &error);
    if (!error.isEmpty()) {
        setLastError(error);
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), error}};
    }
    applyConfig(QJsonObject::fromVariantMap(
        result.value(QStringLiteral("config")).toMap()));
    m_session_id.clear();
    setGenerating(false);
    emit sessionChanged();
    emit sessionCleared();
    refreshSessions();
    setLastError(QString());
    emit toastRequested(activate
                            ? QStringLiteral("供应商已保存并切换")
                            : QStringLiteral("供应商已保存"));
    return result;
}

QVariantMap AishBridge::deleteProvider(const QString &name) {
    if (!ensureService())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), lastError()}};
    QString error;
    const QVariantMap result = parseObject(
        toQString(delete_provider_account(**m_service, name.toStdString())),
        &error);
    if (!error.isEmpty()) {
        setLastError(error);
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), error}};
    }
    return result;
}

QVariantMap AishBridge::startModelFetch(const QString &apiBase,
                                        const QString &apiKey,
                                        const QString &protocol) {
    if (!ensureService())
        return {{QStringLiteral("ok"), false},
                {QStringLiteral("error"), lastError()}};
    QString error;
    const QVariantMap result = parseObject(
        toQString(model_fetch_start(**m_service, apiBase.toStdString(),
                                    apiKey.toStdString(), protocol.toStdString())),
        &error);
    if (!error.isEmpty()) {
        setLastError(error);
        return {{QStringLiteral("ok"), false}, {QStringLiteral("error"), error}};
    }
    return result;
}

void AishBridge::openExternal(const QString &url) {
    const QUrl parsed(url);
    const QString scheme = parsed.scheme().toLower();
    if (!parsed.isValid() ||
        (scheme != QStringLiteral("http") &&
         scheme != QStringLiteral("https") &&
         scheme != QStringLiteral("mailto"))) {
        emit toastRequested(QStringLiteral("已阻止不安全的链接"));
        return;
    }
    if (!QDesktopServices::openUrl(parsed))
        emit toastRequested(QStringLiteral("无法打开链接"));
}

void AishBridge::openConfigFile() {
    const QString path = configPath();
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        setLastError(QStringLiteral("配置文件尚不存在"));
        return;
    }
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void AishBridge::openWorkspace() {
    QDesktopServices::openUrl(QUrl::fromLocalFile(QDir::currentPath()));
}

void AishBridge::copyText(const QString &text) {
    if (text.isEmpty())
        return;
    QGuiApplication::clipboard()->setText(text.left(1024 * 1024));
    emit toastRequested(QStringLiteral("已复制"));
}

QString AishBridge::localFilePath(const QUrl &url) const {
    if (!url.isLocalFile())
        return {};
    const QFileInfo info(url.toLocalFile());
    const QString canonical = info.canonicalFilePath();
    if (canonical.isEmpty() || !QFileInfo(canonical).isFile())
        return {};
    return canonical;
}

QString AishBridge::localDirectory(const QUrl &url) const {
    if (!url.isLocalFile())
        return {};
    const QFileInfo info(url.toLocalFile());
    const QString canonical = info.canonicalFilePath();
    if (canonical.isEmpty() || !QFileInfo(canonical).isDir())
        return {};
    return canonical;
}

QString AishBridge::imageUrl(const QString &path) const {
    const QFileInfo info(path);
    const QString canonical = info.canonicalFilePath();
    if (canonical.isEmpty() || !QFileInfo(canonical).isFile())
        return {};
    return QUrl::fromLocalFile(canonical).toString();
}

QString AishBridge::takeScreenshot() {
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        setLastError(QStringLiteral("无法访问屏幕"));
        return {};
    }
    const QPixmap pixmap = screen->grabWindow(0);
    if (pixmap.isNull()) {
        setLastError(QStringLiteral("屏幕截图失败（当前桌面环境可能不允许截图）"));
        return {};
    }

    const QString base =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
        QStringLiteral("/uos-ai-libre");
    QDir directory(base);
    if (!directory.exists() && !QDir().mkpath(base)) {
        setLastError(QStringLiteral("无法创建截图临时目录"));
        return {};
    }
    QFile::setPermissions(base, QFileDevice::ReadOwner |
                                   QFileDevice::WriteOwner |
                                   QFileDevice::ExeOwner);
    const QString path =
        base + QStringLiteral("/screenshot-%1.png")
                   .arg(QDateTime::currentMSecsSinceEpoch());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        setLastError(QStringLiteral("无法保存截图"));
        return {};
    }
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (!pixmap.save(&file, "PNG")) {
        file.close();
        QFile::remove(path);
        setLastError(QStringLiteral("无法编码截图"));
        return {};
    }
    file.close();
    return path;
}

void AishBridge::cleanupOldScreenshots() {
    const QString base =
        QStandardPaths::writableLocation(QStandardPaths::TempLocation) +
        QStringLiteral("/uos-ai-libre");
    QDir directory(base);
    if (!directory.exists())
        return;
    const QDateTime cutoff = QDateTime::currentDateTime().addDays(-1);
    QFileInfoList files = directory.entryInfoList(
        {QStringLiteral("screenshot-*.png")}, QDir::Files, QDir::Time);
    for (int i = 0; i < files.size(); ++i) {
        if (i >= 8 || files.at(i).lastModified() < cutoff)
            QFile::remove(files.at(i).filePath());
    }
}

void AishBridge::pollEvents() {
    if (!m_service)
        return;
    // Drain in bounded batches so one long-running tool cannot starve the UI
    // event loop. The remaining events stay queued on the Rust side.
    for (int i = 0; i < 256; ++i) {
        const QString raw = toQString(poll_event(**m_service));
        if (raw.isEmpty())
            break;
        QJsonParseError parse_error;
        const QJsonDocument document =
            QJsonDocument::fromJson(raw.toUtf8(), &parse_error);
        if (parse_error.error != QJsonParseError::NoError ||
            !document.isObject())
            continue;
        handleEvent(document.object());
    }
}

void AishBridge::scheduleFlush() {
    if (!m_flush_timer.isActive())
        m_flush_timer.start();
}

void AishBridge::flushPending() {
    if (!m_pending_reasoning.isEmpty()) {
        const QString delta = m_pending_reasoning;
        m_pending_reasoning.clear();
        emit reasoningDelta(delta);
    }
    if (!m_pending_content.isEmpty()) {
        const QString delta = m_pending_content;
        m_pending_content.clear();
        emit contentDelta(delta);
    }
}

void AishBridge::handleEvent(const QJsonObject &event) {
    const QString kind = event.value(QStringLiteral("kind")).toString();
    const QString event_session =
        event.value(QStringLiteral("session_id")).toString();
    if (kind.isEmpty())
        return;

    if (kind == QStringLiteral("settings_job")) {
        emit settingsJobFinished(
            event.value(QStringLiteral("data")).toObject().toVariantMap());
        return;
    }

    if (event_session.isEmpty() || m_session_id.isEmpty() ||
        event_session != m_session_id) {
        // A stale approval must not leave the Rust worker blocked for five
        // minutes. Deny it explicitly; other stale turn events are ignored.
        if (kind == QStringLiteral("approval_request")) {
            const QString request_id =
                event.value(QStringLiteral("data"))
                    .toObject()
                    .value(QStringLiteral("request_id"))
                    .toString();
            if (!request_id.isEmpty() && m_service)
                ::approve(**m_service, request_id.toStdString(), 3);
        }
        return;
    }

    const QVariantMap data =
        event.value(QStringLiteral("data")).toObject().toVariantMap();

    if (kind == QStringLiteral("content_delta")) {
        const QString delta =
            data.value(QStringLiteral("delta")).toString();
        if (!delta.isEmpty()) {
            m_pending_content += delta;
            scheduleFlush();
        }
        return;
    }
    if (kind == QStringLiteral("reasoning_delta")) {
        const QString delta =
            data.value(QStringLiteral("delta")).toString();
        if (!delta.isEmpty()) {
            m_pending_reasoning += delta;
            scheduleFlush();
        }
        return;
    }

    flushPending();

    if (kind == QStringLiteral("turn_start")) {
        m_turn_started_at = QDateTime::currentMSecsSinceEpoch();
        setGenerating(true);
        emit turnStarted(data);
    } else if (kind == QStringLiteral("reasoning_start")) {
        emit reasoningStart();
    } else if (kind == QStringLiteral("reasoning_end")) {
        emit reasoningEnd();
    } else if (kind == QStringLiteral("tool_start")) {
        emit toolStart(data);
    } else if (kind == QStringLiteral("tool_end")) {
        emit toolEnd(data);
    } else if (kind == QStringLiteral("approval_request")) {
        emit approvalRequested(data);
    } else if (kind == QStringLiteral("interaction_required")) {
        emit interactionRequested(data);
    } else if (kind == QStringLiteral("turn_done")) {
        setGenerating(false);
        emit turnDone(data);
        QTimer::singleShot(400, this, &AishBridge::refreshSessions);
    } else if (kind == QStringLiteral("cancelled")) {
        setGenerating(false);
        emit turnCancelled();
        QTimer::singleShot(400, this, &AishBridge::refreshSessions);
    } else if (kind == QStringLiteral("error")) {
        QString message = data.value(QStringLiteral("message")).toString();
        if (message.isEmpty())
            message = QStringLiteral("AI 请求失败");
        setGenerating(false);
        setLastError(message);
        emit turnError(message);
    }
}
