#pragma once

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>
#include <QVariantList>
#include <QVector>

/// The conversation model used by the chat page.
///
/// Streaming updates touch only the current assistant row, and assistant text
/// is rendered by a per-bubble MarkdownModel rather than rebuilding the entire
/// message list.
class ChatModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool hasMessages READ hasMessages NOTIFY countChanged)
    Q_PROPERTY(int streamingIndex READ streamingIndex NOTIFY streamingChanged)

public:
    enum Role {
        KindRole = Qt::UserRole + 1,
        TextRole,
        ReasoningRole,
        ReasoningActiveRole,
        ReasoningElapsedRole,
        StreamingRole,
        ToolsRole,
        ElapsedRole,
        ModelRole,
        ErrorRole,
        NoticeRole,
        AttachmentsRole,
        TimestampRole,
    };
    Q_ENUM(Role)

    explicit ChatModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    int count() const;
    bool hasMessages() const;
    int streamingIndex() const;

    Q_INVOKABLE void clear();
    Q_INVOKABLE void appendUser(const QString &text,
                                const QVariantList &attachments = {});
    Q_INVOKABLE int beginAssistant(const QString &model = {});
    Q_INVOKABLE void appendContent(const QString &delta);
    Q_INVOKABLE void setContent(const QString &text);
    Q_INVOKABLE void beginReasoning();
    Q_INVOKABLE void appendReasoning(const QString &delta);
    Q_INVOKABLE void endReasoning();
    Q_INVOKABLE void addToolStart(const QVariantMap &data);
    Q_INVOKABLE void addToolEnd(const QVariantMap &data);
    Q_INVOKABLE void finishAssistant();
    Q_INVOKABLE void failAssistant(const QString &message);
    Q_INVOKABLE void cancelAssistant();
    Q_INVOKABLE void loadTranscript(const QVariantList &messages);
    Q_INVOKABLE QString lastUserText() const;
    Q_INVOKABLE int lastUserRow() const;
    Q_INVOKABLE QVariantList lastUserAttachments() const;

signals:
    void countChanged();
    void streamingChanged();
    void contentChanged();

private:
    struct Tool {
        QString id;
        QString name;
        QString target;
        QString args;
        QString result;
        QString status; // running, done, failed
        int durationMs = 0;
    };

    struct Message {
        QString kind; // user or assistant
        QString text;
        QString reasoning;
        QString model;
        QString error;
        QString notice;
        bool streaming = false;
        bool reasoningActive = false;
        qint64 reasoningStartedAt = 0;
        qint64 reasoningElapsedMs = 0;
        qint64 turnStartedAt = 0;
        qint64 elapsedMs = 0;
        qint64 timestamp = 0;
        QVariantList attachments;
        QVector<Tool> tools;
    };

    static QString contentText(const QVariant &content);
    static QString compactValue(const QVariant &value);
    static QString toolTarget(const QString &name, const QVariantMap &args);
    static QVector<Tool> parseToolCalls(const QVariant &calls);
    static QVariantList toolsToVariant(const QVector<Tool> &tools);

    void emitRowChanged(int row, const QVector<int> &roles);
    void setToolStatus(const QString &id, const QString &status,
                       const QString &result, int durationMs);
    int findToolMessage(const QString &id) const;

    QVector<Message> m_messages;
    int m_streaming_index = -1;
};
