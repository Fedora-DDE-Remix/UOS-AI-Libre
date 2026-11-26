#include "chatmodel.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>

namespace {

constexpr int kMaxUserChars = 64 * 1024;
constexpr int kMaxAssistantChars = 512 * 1024;
constexpr int kMaxReasoningChars = 128 * 1024;
constexpr int kMaxToolPreviewChars = 800;
constexpr int kMaxToolResultChars = 16000;

QString bounded(const QString &text, int limit) {
    if (text.size() <= limit)
        return text;
    QString result = text.left(limit);
    result += QStringLiteral("…");
    return result;
}

QString formatArguments(const QVariant &arguments) {
    if (arguments.metaType().id() == QMetaType::QString) {
        const QString raw = arguments.toString();
        const QJsonDocument doc = QJsonDocument::fromJson(raw.toUtf8());
        if (doc.isObject()) {
            return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
        }
        return raw;
    }
    const QJsonDocument doc = QJsonDocument::fromVariant(arguments);
    if (!doc.isNull())
        return QString::fromUtf8(doc.toJson(QJsonDocument::Compact));
    return arguments.toString();
}

} // namespace

ChatModel::ChatModel(QObject *parent)
    : QAbstractListModel(parent) {}

int ChatModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_messages.size();
}

int ChatModel::count() const { return m_messages.size(); }

bool ChatModel::hasMessages() const { return !m_messages.isEmpty(); }

int ChatModel::streamingIndex() const { return m_streaming_index; }

QHash<int, QByteArray> ChatModel::roleNames() const {
    return {
        {KindRole, "kind"},
        {TextRole, "text"},
        {ReasoningRole, "reasoning"},
        {ReasoningActiveRole, "reasoningActive"},
        {ReasoningElapsedRole, "reasoningElapsed"},
        {StreamingRole, "streaming"},
        {ToolsRole, "tools"},
        {ElapsedRole, "elapsedMs"},
        {ModelRole, "modelName"},
        {ErrorRole, "error"},
        {NoticeRole, "notice"},
        {AttachmentsRole, "attachments"},
        {TimestampRole, "timestamp"},
    };
}

QVariant ChatModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_messages.size())
        return {};
    const Message &message = m_messages.at(index.row());
    switch (role) {
    case KindRole:
        return message.kind;
    case TextRole:
        return message.text;
    case ReasoningRole:
        return message.reasoning;
    case ReasoningActiveRole:
        return message.reasoningActive;
    case ReasoningElapsedRole:
        if (message.reasoningActive && message.reasoningStartedAt > 0)
            return QDateTime::currentMSecsSinceEpoch() - message.reasoningStartedAt;
        return message.reasoningElapsedMs;
    case StreamingRole:
        return message.streaming;
    case ToolsRole:
        return toolsToVariant(message.tools);
    case ElapsedRole:
        if (message.streaming && message.turnStartedAt > 0)
            return QDateTime::currentMSecsSinceEpoch() - message.turnStartedAt;
        return message.elapsedMs;
    case ModelRole:
        return message.model;
    case ErrorRole:
        return message.error;
    case NoticeRole:
        return message.notice;
    case AttachmentsRole:
        return message.attachments;
    case TimestampRole:
        return message.timestamp;
    default:
        return {};
    }
}

void ChatModel::emitRowChanged(int row, const QVector<int> &roles) {
    if (row < 0 || row >= m_messages.size())
        return;
    emit dataChanged(index(row), index(row), roles);
}

void ChatModel::clear() {
    if (m_messages.isEmpty() && m_streaming_index < 0)
        return;
    beginResetModel();
    m_messages.clear();
    m_streaming_index = -1;
    endResetModel();
    emit countChanged();
    emit streamingChanged();
    emit contentChanged();
}

void ChatModel::appendUser(const QString &text,
                           const QVariantList &attachments) {
    if (text.trimmed().isEmpty() && attachments.isEmpty())
        return;
    Message message;
    message.kind = QStringLiteral("user");
    message.text = bounded(text, kMaxUserChars);
    message.timestamp = QDateTime::currentMSecsSinceEpoch();
    message.attachments = attachments.mid(0, 4);
    const int row = m_messages.size();
    beginInsertRows(QModelIndex(), row, row);
    m_messages.append(message);
    endInsertRows();
    emit countChanged();
    emit contentChanged();
}

int ChatModel::beginAssistant(const QString &model) {
    if (m_streaming_index >= 0)
        finishAssistant();

    Message message;
    message.kind = QStringLiteral("assistant");
    message.model = model;
    message.streaming = true;
    message.turnStartedAt = QDateTime::currentMSecsSinceEpoch();
    message.timestamp = message.turnStartedAt;
    const int row = m_messages.size();
    beginInsertRows(QModelIndex(), row, row);
    m_messages.append(message);
    endInsertRows();
    m_streaming_index = row;
    emit countChanged();
    emit streamingChanged();
    emit contentChanged();
    return row;
}

void ChatModel::appendContent(const QString &delta) {
    if (delta.isEmpty())
        return;
    if (m_streaming_index < 0)
        beginAssistant({});
    if (m_streaming_index < 0)
        return;
    Message &message = m_messages[m_streaming_index];
    if (message.reasoningActive)
        endReasoning();
    message.text += delta;
    if (message.text.size() > kMaxAssistantChars)
        message.text = bounded(message.text, kMaxAssistantChars);
    emitRowChanged(m_streaming_index, {TextRole});
    emit contentChanged();
}

void ChatModel::setContent(const QString &text) {
    if (m_streaming_index < 0)
        return;
    if (text.isEmpty())
        return;
    Message &message = m_messages[m_streaming_index];
    const QString bounded_text = bounded(text, kMaxAssistantChars);
    if (message.text == bounded_text)
        return;
    message.text = bounded_text;
    emitRowChanged(m_streaming_index, {TextRole});
    emit contentChanged();
}

void ChatModel::beginReasoning() {
    if (m_streaming_index < 0)
        beginAssistant({});
    if (m_streaming_index < 0)
        return;
    Message &message = m_messages[m_streaming_index];
    if (message.reasoningActive)
        return;
    message.reasoningActive = true;
    message.reasoningStartedAt = QDateTime::currentMSecsSinceEpoch();
    message.reasoningElapsedMs = 0;
    emitRowChanged(m_streaming_index,
                   {ReasoningActiveRole, ReasoningElapsedRole});
    emit contentChanged();
}

void ChatModel::appendReasoning(const QString &delta) {
    if (delta.isEmpty())
        return;
    if (m_streaming_index < 0)
        beginAssistant({});
    if (m_streaming_index < 0)
        return;
    Message &message = m_messages[m_streaming_index];
    if (!message.reasoningActive)
        beginReasoning();
    message.reasoning += delta;
    if (message.reasoning.size() > kMaxReasoningChars)
        message.reasoning = bounded(message.reasoning, kMaxReasoningChars);
    emitRowChanged(m_streaming_index,
                   {ReasoningRole, ReasoningElapsedRole});
    emit contentChanged();
}

void ChatModel::endReasoning() {
    if (m_streaming_index < 0)
        return;
    Message &message = m_messages[m_streaming_index];
    if (!message.reasoningActive)
        return;
    message.reasoningActive = false;
    if (message.reasoningStartedAt > 0)
        message.reasoningElapsedMs =
            QDateTime::currentMSecsSinceEpoch() - message.reasoningStartedAt;
    emitRowChanged(m_streaming_index,
                   {ReasoningActiveRole, ReasoningElapsedRole});
    emit contentChanged();
}

QString ChatModel::compactValue(const QVariant &value) {
    return formatArguments(value);
}

QString ChatModel::toolTarget(const QString &name, const QVariantMap &args) {
    Q_UNUSED(name)
    static const QStringList keys = {
        QStringLiteral("url"),      QStringLiteral("query"),
        QStringLiteral("command"),  QStringLiteral("path"),
        QStringLiteral("pattern"),  QStringLiteral("prompt"),
        QStringLiteral("file_path"), QStringLiteral("target"),
        QStringLiteral("source"),
    };
    for (const QString &key : keys) {
        if (!args.contains(key))
            continue;
        QString value = args.value(key).toString().simplified();
        if (!value.isEmpty())
            return bounded(value, 240);
    }
    const QString compact = compactValue(args).simplified();
    return bounded(compact, 240);
}

void ChatModel::addToolStart(const QVariantMap &data) {
    if (m_streaming_index < 0)
        beginAssistant({});
    if (m_streaming_index < 0)
        return;
    Tool tool;
    tool.id = data.value(QStringLiteral("tool_call_id")).toString();
    tool.name = data.value(QStringLiteral("tool_name")).toString();
    if (tool.name.isEmpty())
        tool.name = QStringLiteral("工具");
    const QVariantMap args = data.value(QStringLiteral("tool_args")).toMap();
    tool.target = toolTarget(tool.name, args);
    tool.args = bounded(compactValue(args), kMaxToolPreviewChars);
    tool.status = QStringLiteral("running");

    Message &message = m_messages[m_streaming_index];
    bool replaced = false;
    if (!tool.id.isEmpty()) {
        for (Tool &existing : message.tools) {
            if (existing.id == tool.id) {
                existing = tool;
                replaced = true;
                break;
            }
        }
    }
    if (!replaced)
        message.tools.append(tool);
    emitRowChanged(m_streaming_index, {ToolsRole, ElapsedRole});
    emit contentChanged();
}

void ChatModel::addToolEnd(const QVariantMap &data) {
    const QString id = data.value(QStringLiteral("tool_call_id")).toString();
    const bool ok = data.value(QStringLiteral("ok"), true).toBool();
    const int duration = data.value(QStringLiteral("duration_ms")).toInt();
    const QVariantMap meta = data.value(QStringLiteral("meta")).toMap();
    QString result;
    if (meta.contains(QStringLiteral("result")))
        result = meta.value(QStringLiteral("result")).toString();
    else if (meta.contains(QStringLiteral("error")))
        result = meta.value(QStringLiteral("error")).toString();
    else if (meta.contains(QStringLiteral("code")))
        result = QStringLiteral("HTTP %1 %2")
                     .arg(meta.value(QStringLiteral("code")).toInt())
                     .arg(meta.value(QStringLiteral("codeText")).toString());
    if (result.isEmpty())
        result = data.value(QStringLiteral("error")).toString();
    result = bounded(result.trimmed(), kMaxToolResultChars);

    const QString status = ok ? QStringLiteral("done") : QStringLiteral("failed");
    int row = m_streaming_index;
    if (row < 0)
        row = findToolMessage(id);
    if (row < 0) {
        beginAssistant({});
        row = m_streaming_index;
    }
    if (row < 0)
        return;

    Message &message = m_messages[row];
    bool found = false;
    for (Tool &tool : message.tools) {
        if (!id.isEmpty() && tool.id == id) {
            tool.status = status;
            tool.result = result;
            tool.durationMs = duration;
            found = true;
            break;
        }
    }
    if (!found) {
        Tool tool;
        tool.id = id;
        tool.name = data.value(QStringLiteral("tool_name")).toString();
        if (tool.name.isEmpty())
            tool.name = QStringLiteral("工具");
        const QVariantMap args = data.value(QStringLiteral("tool_args")).toMap();
        tool.target = toolTarget(tool.name, args);
        tool.args = bounded(compactValue(args), kMaxToolPreviewChars);
        tool.status = status;
        tool.result = result;
        tool.durationMs = duration;
        message.tools.append(tool);
    }
    emitRowChanged(row, {ToolsRole});
    emit contentChanged();
}

int ChatModel::findToolMessage(const QString &id) const {
    if (id.isEmpty())
        return -1;
    for (int i = m_messages.size() - 1; i >= 0; --i) {
        for (const Tool &tool : m_messages.at(i).tools) {
            if (tool.id == id)
                return i;
        }
    }
    return -1;
}

void ChatModel::finishAssistant() {
    if (m_streaming_index < 0)
        return;
    Message &message = m_messages[m_streaming_index];
    if (message.reasoningActive) {
        message.reasoningActive = false;
        if (message.reasoningStartedAt > 0)
            message.reasoningElapsedMs =
                QDateTime::currentMSecsSinceEpoch() - message.reasoningStartedAt;
    }
    message.streaming = false;
    if (message.turnStartedAt > 0)
        message.elapsedMs =
            QDateTime::currentMSecsSinceEpoch() - message.turnStartedAt;
    const int row = m_streaming_index;
    m_streaming_index = -1;
    emit dataChanged(index(row), index(row));
    emit streamingChanged();
    emit contentChanged();
}

void ChatModel::failAssistant(const QString &message_text) {
    if (m_streaming_index < 0)
        beginAssistant({});
    if (m_streaming_index < 0)
        return;
    Message &message = m_messages[m_streaming_index];
    message.error = message_text;
    message.notice.clear();
    finishAssistant();
}

void ChatModel::cancelAssistant() {
    if (m_streaming_index < 0)
        return;
    Message &message = m_messages[m_streaming_index];
    message.notice = QStringLiteral("已停止生成");
    message.error.clear();
    finishAssistant();
}

int ChatModel::lastUserRow() const {
    for (int i = m_messages.size() - 1; i >= 0; --i) {
        if (m_messages.at(i).kind == QStringLiteral("user"))
            return i;
    }
    return -1;
}

QString ChatModel::lastUserText() const {
    for (int i = m_messages.size() - 1; i >= 0; --i) {
        if (m_messages.at(i).kind == QStringLiteral("user"))
            return m_messages.at(i).text;
    }
    return {};
}

QVariantList ChatModel::lastUserAttachments() const {
    for (int i = m_messages.size() - 1; i >= 0; --i) {
        if (m_messages.at(i).kind == QStringLiteral("user"))
            return m_messages.at(i).attachments;
    }
    return {};
}

QVariantList ChatModel::toolsToVariant(const QVector<Tool> &tools) {
    QVariantList result;
    result.reserve(tools.size());
    for (const Tool &tool : tools) {
        QVariantMap item;
        item.insert(QStringLiteral("id"), tool.id);
        item.insert(QStringLiteral("name"), tool.name);
        item.insert(QStringLiteral("target"), tool.target);
        item.insert(QStringLiteral("args"), tool.args);
        item.insert(QStringLiteral("result"), tool.result);
        item.insert(QStringLiteral("status"), tool.status);
        item.insert(QStringLiteral("durationMs"), tool.durationMs);
        result.append(item);
    }
    return result;
}

QString ChatModel::contentText(const QVariant &content) {
    if (content.metaType().id() == QMetaType::QString)
        return content.toString();
    if (content.metaType().id() == QMetaType::QVariantList) {
        QString text;
        const QVariantList blocks = content.toList();
        for (const QVariant &block : blocks) {
            const QVariantMap map = block.toMap();
            const QString type = map.value(QStringLiteral("type")).toString();
            if (type == QStringLiteral("text"))
                text += map.value(QStringLiteral("text")).toString();
            else if (type.contains(QStringLiteral("image")))
                text += QStringLiteral("[图片]");
        }
        return text;
    }
    return content.toString();
}

QVector<ChatModel::Tool> ChatModel::parseToolCalls(const QVariant &calls) {
    QVector<Tool> result;
    if (calls.metaType().id() != QMetaType::QVariantList)
        return result;
    const QVariantList list = calls.toList();
    for (const QVariant &entry : list) {
        const QVariantMap map = entry.toMap();
        Tool tool;
        tool.id = map.value(QStringLiteral("id")).toString();
        tool.name = map.value(QStringLiteral("name")).toString();
        if (tool.name.isEmpty())
            tool.name = QStringLiteral("工具");
        QVariant arguments = map.value(QStringLiteral("arguments"));
        QVariantMap args;
        if (arguments.metaType().id() == QMetaType::QString) {
            const QJsonDocument doc =
                QJsonDocument::fromJson(arguments.toString().toUtf8());
            if (doc.isObject())
                args = doc.object().toVariantMap();
        } else if (arguments.canConvert<QVariantMap>()) {
            args = arguments.toMap();
        }
        tool.target = toolTarget(tool.name, args);
        tool.args = bounded(compactValue(args), kMaxToolPreviewChars);
        tool.status = QStringLiteral("running");
        result.append(tool);
    }
    return result;
}

void ChatModel::loadTranscript(const QVariantList &messages) {
    beginResetModel();
    m_messages.clear();
    m_streaming_index = -1;

    QVector<Tool> pending_tools;
    QString pending_reasoning;
    for (const QVariant &entry : messages) {
        const QVariantMap map = entry.toMap();
        const QString role = map.value(QStringLiteral("role")).toString();

        if (role == QStringLiteral("user")) {
            Message message;
            message.kind = QStringLiteral("user");
            message.text = bounded(contentText(map.value(QStringLiteral("content"))),
                                   kMaxUserChars);
            if (message.text.trimmed().isEmpty())
                message.text = QStringLiteral("[图片]");
            m_messages.append(message);
            continue;
        }

        if (role == QStringLiteral("tool")) {
            const QString id = map.value(QStringLiteral("tool_call_id")).toString();
            const QString result =
                bounded(contentText(map.value(QStringLiteral("content"))).simplified(),
                        kMaxToolResultChars);
            bool updated = false;
            for (Tool &tool : pending_tools) {
                if (!id.isEmpty() && tool.id == id) {
                    tool.status = QStringLiteral("done");
                    tool.result = result;
                    updated = true;
                    break;
                }
            }
            if (!updated) {
                for (int i = m_messages.size() - 1; i >= 0; --i) {
                    for (Tool &tool : m_messages[i].tools) {
                        if (!id.isEmpty() && tool.id == id) {
                            tool.status = QStringLiteral("done");
                            tool.result = result;
                            updated = true;
                            break;
                        }
                    }
                    if (updated)
                        break;
                }
            }
            continue;
        }

        if (role != QStringLiteral("assistant"))
            continue;

        const QString text =
            bounded(contentText(map.value(QStringLiteral("content"))),
                    kMaxAssistantChars);
        const QString reasoning =
            map.value(QStringLiteral("reasoning_content")).toString();
        const QVector<Tool> tools =
            parseToolCalls(map.value(QStringLiteral("tool_calls")));

        if (text.trimmed().isEmpty() && !tools.isEmpty()) {
            pending_tools += tools;
            if (!reasoning.isEmpty())
                pending_reasoning += reasoning;
            continue;
        }
        if (text.trimmed().isEmpty() && tools.isEmpty() && reasoning.isEmpty())
            continue;

        Message message;
        message.kind = QStringLiteral("assistant");
        message.text = text;
        message.reasoning = pending_reasoning + reasoning;
        message.tools = pending_tools + tools;
        pending_tools.clear();
        pending_reasoning.clear();
        m_messages.append(message);
    }

    if (!pending_tools.isEmpty()) {
        for (Tool &tool : pending_tools) {
            if (tool.status == QStringLiteral("running"))
                tool.status = QStringLiteral("done");
        }
        Message message;
        message.kind = QStringLiteral("assistant");
        message.reasoning = pending_reasoning;
        message.tools = pending_tools;
        m_messages.append(message);
    }

    for (Message &message : m_messages) {
        for (Tool &tool : message.tools) {
            if (tool.status == QStringLiteral("running"))
                tool.status = QStringLiteral("done");
        }
    }

    endResetModel();
    emit countChanged();
    emit streamingChanged();
    emit contentChanged();
}
