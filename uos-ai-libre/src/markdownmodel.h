#pragma once

#include <QAbstractListModel>
#include <QtQml/qqmlregistration.h>
#include <QString>
#include <QVariantList>
#include <QVector>

/// Parses a safe subset of GitHub-flavoured Markdown into layout blocks.
///
/// The model deliberately renders no HTML and performs no network I/O. Each
/// block is a separate QML delegate, so a streaming answer only re-lays out the
/// changed tail instead of rebuilding the whole message.
class MarkdownModel : public QAbstractListModel {
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY textChanged)
    Q_PROPERTY(bool truncated READ truncated NOTIFY truncatedChanged)

public:
    enum Role {
        TypeRole = Qt::UserRole + 1,
        TextRole,
        LevelRole,
        MarkerRole,
        LangRole,
        RowsRole,
        AlignmentsRole,
        IndentRole,
        CheckedRole,
        ClosedRole,
        ColumnWidthsRole,
    };
    Q_ENUM(Role)

    explicit MarkdownModel(QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString text() const;
    void setText(const QString &text);
    bool truncated() const;

signals:
    void textChanged();
    void truncatedChanged();

private:
    struct Block {
        QString type;
        QString text;
        QString marker;
        QString lang;
        int level = 0;
        int indent = 0;
        int checked = -1;
        bool closed = true;
        QVariantList rows;
        QVariantList alignments;
        QVariantList columnWidths;
    };

    static bool sameBlock(const Block &a, const Block &b);
    static QVector<Block> parseBlocks(const QString &text);
    void applyBlocks(const QVector<Block> &blocks);

    QString m_text;
    QVector<Block> m_blocks;
    bool m_truncated = false;
};
