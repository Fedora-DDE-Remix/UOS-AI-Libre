#include "markdownmodel.h"

#include <QRegularExpression>
#include <QStringList>

namespace {

constexpr int kMaxMarkdownChars = 300 * 1024;
constexpr int kMaxBlocks = 2000;
constexpr int kMaxTableColumns = 12;
constexpr int kMaxTableRows = 120;
constexpr int kMaxParagraphChars = 4000;

bool isCjk(QChar ch) {
    const ushort u = ch.unicode();
    return (u >= 0x2E80 && u <= 0x9FFF) || (u >= 0xF900 && u <= 0xFAFF) ||
           (u >= 0xFE30 && u <= 0xFE4F) || (u >= 0xFF00 && u <= 0xFFEF) ||
           (u >= 0x3000 && u <= 0x303F);
}

int displayLength(const QString &text) {
    int width = 0;
    for (const QChar ch : text)
        width += isCjk(ch) ? 2 : 1;
    return width;
}

int leadingIndent(const QString &line) {
    int spaces = 0;
    for (const QChar ch : line) {
        if (ch == QLatin1Char(' '))
            ++spaces;
        else if (ch == QLatin1Char('\t'))
            spaces += 2;
        else
            break;
    }
    return spaces;
}

QString joinSoftLines(const QStringList &lines) {
    QString joined;
    for (const QString &raw : lines) {
        const QString line = raw.trimmed();
        if (line.isEmpty())
            continue;
        if (!joined.isEmpty()) {
            const QChar left = joined.back();
            const QChar right = line.front();
            if (!(isCjk(left) || isCjk(right)))
                joined += QLatin1Char(' ');
        }
        joined += line;
    }
    return joined;
}

QString sanitizeInline(const QString &input) {
    // Qt's Markdown importer must never be asked to load remote images. The
    // model turns them into a plain label instead.
    static const QRegularExpression image_re(
        QStringLiteral(R"(!\[([^\]]*)\]\([^)]*\))"));
    QString text = input;
    text.replace(image_re, QStringLiteral("[图片]"));
    // Qt's Markdown importer may accept raw HTML depending on the feature
    // flags. Escape the tag opener so a model cannot inject HTML or trigger
    // resource loads through inline markup. Code fences are rendered as plain
    // text and are not passed through this function.
    text.replace(QLatin1Char('<'), QStringLiteral("&lt;"));
    return text;
}

bool isFenceClose(const QString &line, QChar fence_char, int fence_length) {
    const QString trimmed = line.trimmed();
    int count = 0;
    while (count < trimmed.size() && trimmed.at(count) == fence_char)
        ++count;
    if (count < fence_length)
        return false;
    for (int i = count; i < trimmed.size(); ++i) {
        if (trimmed.at(i) != QLatin1Char(' ') && trimmed.at(i) != QLatin1Char('\t'))
            return false;
    }
    return true;
}

QStringList splitTableRow(const QString &line) {
    QString text = line.trimmed();
    if (text.startsWith(QLatin1Char('|')))
        text.remove(0, 1);
    if (text.endsWith(QLatin1Char('|')) && !text.endsWith(QStringLiteral("\\|")))
        text.chop(1);

    QStringList cells;
    QString current;
    bool in_code = false;
    for (int i = 0; i < text.size(); ++i) {
        const QChar ch = text.at(i);
        if (ch == QLatin1Char('\\') && i + 1 < text.size() &&
            text.at(i + 1) == QLatin1Char('|')) {
            current += QLatin1Char('|');
            ++i;
            continue;
        }
        if (ch == QLatin1Char('`'))
            in_code = !in_code;
        if (ch == QLatin1Char('|') && !in_code) {
            cells.append(current.trimmed());
            current.clear();
        } else {
            current += ch;
        }
    }
    cells.append(current.trimmed());
    return cells;
}

bool isTableSeparator(const QString &line) {
    if (!line.contains(QLatin1Char('|')))
        return false;
    const QStringList cells = splitTableRow(line);
    if (cells.isEmpty())
        return false;
    static const QRegularExpression sep(
        QStringLiteral(R"(^:?-{3,}:?$)"));
    for (const QString &cell : cells) {
        if (!sep.match(cell.trimmed()).hasMatch())
            return false;
    }
    return true;
}

QVariantList rowsToVariant(const QVector<QStringList> &rows, int columns) {
    QVariantList result;
    result.reserve(rows.size());
    for (const QStringList &row : rows) {
        QVariantList cells;
        cells.reserve(columns);
        for (int i = 0; i < columns; ++i)
            cells.append(i < row.size() ? row.at(i) : QString());
        result.append(QVariant::fromValue(cells));
    }
    return result;
}

QVariantList alignmentsToVariant(const QStringList &cells) {
    QVariantList result;
    for (const QString &cell : cells) {
        const QString trimmed = cell.trimmed();
        const bool left = trimmed.startsWith(QLatin1Char(':'));
        const bool right = trimmed.endsWith(QLatin1Char(':'));
        if (left && right)
            result.append(1);
        else if (right)
            result.append(2);
        else
            result.append(0);
    }
    return result;
}

QVariantList columnWeights(const QVector<QStringList> &rows, int columns) {
    QVector<int> lengths(columns, 6);
    for (const QStringList &row : rows) {
        for (int i = 0; i < columns && i < row.size(); ++i)
            lengths[i] = qMax(lengths[i], qMin(40, displayLength(row.at(i))));
    }
    double total = 0;
    for (int value : lengths)
        total += value;
    QVariantList result;
    if (total <= 0)
        return result;
    for (int value : lengths)
        result.append(double(value) / total);
    return result;
}

} // namespace

MarkdownModel::MarkdownModel(QObject *parent)
    : QAbstractListModel(parent) {}

int MarkdownModel::rowCount(const QModelIndex &parent) const {
    return parent.isValid() ? 0 : m_blocks.size();
}

QVariant MarkdownModel::data(const QModelIndex &index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= m_blocks.size())
        return {};
    const Block &block = m_blocks.at(index.row());
    switch (role) {
    case TypeRole:
        return block.type;
    case TextRole:
        return block.text;
    case LevelRole:
        return block.level;
    case MarkerRole:
        return block.marker;
    case LangRole:
        return block.lang;
    case RowsRole:
        return block.rows;
    case AlignmentsRole:
        return block.alignments;
    case IndentRole:
        return block.indent;
    case CheckedRole:
        return block.checked;
    case ClosedRole:
        return block.closed;
    case ColumnWidthsRole:
        return block.columnWidths;
    default:
        return {};
    }
}

QHash<int, QByteArray> MarkdownModel::roleNames() const {
    return {
        {TypeRole, "type"},
        {TextRole, "text"},
        {LevelRole, "level"},
        {MarkerRole, "marker"},
        {LangRole, "lang"},
        {RowsRole, "rows"},
        {AlignmentsRole, "alignments"},
        {IndentRole, "indent"},
        {CheckedRole, "checked"},
        {ClosedRole, "closed"},
        {ColumnWidthsRole, "columnWidths"},
    };
}

QString MarkdownModel::text() const { return m_text; }

bool MarkdownModel::truncated() const { return m_truncated; }

void MarkdownModel::setText(const QString &value) {
    QString normalized = value;
    normalized.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    normalized.replace(QLatin1Char('\r'), QLatin1Char('\n'));
    bool truncated = false;
    if (normalized.size() > kMaxMarkdownChars) {
        normalized.truncate(kMaxMarkdownChars);
        truncated = true;
    }
    if (normalized == m_text && truncated == m_truncated)
        return;

    const bool text_changed = normalized != m_text;
    m_text = normalized;
    applyBlocks(parseBlocks(m_text));
    if (text_changed)
        emit textChanged();
    if (truncated != m_truncated) {
        m_truncated = truncated;
        emit truncatedChanged();
    }
}

bool MarkdownModel::sameBlock(const Block &a, const Block &b) {
    return a.type == b.type && a.text == b.text && a.marker == b.marker &&
           a.lang == b.lang && a.level == b.level && a.indent == b.indent &&
           a.checked == b.checked && a.closed == b.closed && a.rows == b.rows &&
           a.alignments == b.alignments &&
           a.columnWidths == b.columnWidths;
}

void MarkdownModel::applyBlocks(const QVector<Block> &blocks) {
    const QVector<Block> old = m_blocks;
    int common = 0;
    while (common < old.size() && common < blocks.size() &&
           sameBlock(old.at(common), blocks.at(common)))
        ++common;
    if (common == old.size() && common == blocks.size())
        return;

    if (blocks.size() > old.size()) {
        beginInsertRows(QModelIndex(), old.size(), blocks.size() - 1);
        m_blocks = blocks;
        endInsertRows();
    } else if (blocks.size() < old.size()) {
        beginRemoveRows(QModelIndex(), blocks.size(), old.size() - 1);
        m_blocks = blocks;
        endRemoveRows();
    } else {
        m_blocks = blocks;
    }

    const int changed_end = qMin(old.size(), blocks.size()) - 1;
    if (common <= changed_end)
        emit dataChanged(index(common), index(changed_end));
}

QVector<MarkdownModel::Block> MarkdownModel::parseBlocks(const QString &text) {
    static const QRegularExpression atx_heading(
        QStringLiteral(R"(^ {0,3}(#{1,6})(?:[ \t]+|$)(.*)$)"));
    static const QRegularExpression setext_one(
        QStringLiteral(R"(^ {0,3}=+[ \t]*$)"));
    static const QRegularExpression setext_two(
        QStringLiteral(R"(^ {0,3}-+[ \t]*$)"));
    static const QRegularExpression horizontal_rule(
        QStringLiteral(R"(^ {0,3}(?:(?:\*[ \t]*){3,}|(?:-[ \t]*){3,}|(?:_[ \t]*){3,})$)"));
    static const QRegularExpression fence_re(
        QStringLiteral(R"(^ {0,3}(`{3,}|~{3,})[ \t]*([A-Za-z0-9_+.#-]*)[ \t]*$)"));
    static const QRegularExpression bullet_re(
        QStringLiteral(R"(^([ \t]*)[-+*][ \t]+(.*)$)"));
    static const QRegularExpression ordered_re(
        QStringLiteral(R"(^([ \t]*)(\d{1,9})[.)][ \t]+(.*)$)"));
    static const QRegularExpression quote_re(
        QStringLiteral(R"(^ {0,3}>[ \t]?(.*)$)"));
    static const QRegularExpression task_re(
        QStringLiteral(R"(^\[([ xX])\][ \t]+(.*)$)"));

    QVector<Block> blocks;
    const QStringList lines = text.split(QLatin1Char('\n'), Qt::KeepEmptyParts);
    QStringList paragraph;

    auto flush_paragraph = [&]() {
        if (paragraph.isEmpty())
            return;
        const QString text = sanitizeInline(joinSoftLines(paragraph));
        paragraph.clear();
        if (text.trimmed().isEmpty())
            return;
        int start = 0;
        while (start < text.size()) {
            int length = qMin(kMaxParagraphChars, text.size() - start);
            if (start + length < text.size()) {
                const int space = text.lastIndexOf(QLatin1Char(' '), start + length);
                if (space > start + 1000)
                    length = space - start;
            }
            Block block;
            block.type = QStringLiteral("paragraph");
            block.text = text.mid(start, length);
            blocks.append(block);
            start += length;
            while (start < text.size() && text.at(start) == QLatin1Char(' '))
                ++start;
        }
    };

    for (int i = 0; i < lines.size();) {
        if (blocks.size() >= kMaxBlocks) {
            Block block;
            block.type = QStringLiteral("paragraph");
            block.text = QStringLiteral("…（内容过长，已省略）");
            blocks.append(block);
            break;
        }
        const QString &line = lines.at(i);
        const QString trimmed = line.trimmed();

        if (trimmed.isEmpty()) {
            flush_paragraph();
            ++i;
            continue;
        }

        // Fenced code, including an unterminated fence while streaming.
        const auto fence_match = fence_re.match(line);
        if (fence_match.hasMatch()) {
            flush_paragraph();
            const QString fence = fence_match.captured(1);
            const QChar fence_char = fence.at(0);
            const int fence_length = fence.size();
            const QString lang = fence_match.captured(2);
            QStringList code;
            bool closed = false;
            ++i;
            while (i < lines.size()) {
                if (isFenceClose(lines.at(i), fence_char, fence_length)) {
                    closed = true;
                    ++i;
                    break;
                }
                code.append(lines.at(i));
                ++i;
            }
            Block block;
            block.type = QStringLiteral("code");
            block.text = code.join(QLatin1Char('\n'));
            block.lang = lang;
            block.closed = closed;
            blocks.append(block);
            continue;
        }

        // Display math. Rendering is intentionally simple but never executes
        // macros or loads remote resources.
        if (trimmed.startsWith(QStringLiteral("$$"))) {
            flush_paragraph();
            QString math;
            if (trimmed.size() > 4 && trimmed.endsWith(QStringLiteral("$$"))) {
                math = trimmed.mid(2, trimmed.size() - 4);
                ++i;
            } else {
                ++i;
                while (i < lines.size()) {
                    const QString current = lines.at(i).trimmed();
                    if (current == QStringLiteral("$$")) {
                        ++i;
                        break;
                    }
                    if (current.endsWith(QStringLiteral("$$"))) {
                        math += lines.at(i).left(lines.at(i).lastIndexOf(QStringLiteral("$$")));
                        ++i;
                        break;
                    }
                    math += lines.at(i) + QLatin1Char('\n');
                    ++i;
                }
            }
            Block block;
            block.type = QStringLiteral("math");
            block.text = math.trimmed();
            blocks.append(block);
            continue;
        }

        // Tables need the current header line and the next separator line.
        if (i + 1 < lines.size() && line.contains(QLatin1Char('|')) &&
            isTableSeparator(lines.at(i + 1))) {
            flush_paragraph();
            const QStringList header_cells = splitTableRow(line);
            const QStringList align_cells = splitTableRow(lines.at(i + 1));
            const int columns = qMin(header_cells.size(), kMaxTableColumns);
            QVector<QStringList> rows;
            QStringList header;
            for (int c = 0; c < columns; ++c) {
                const QString cell = c < header_cells.size() ? header_cells.at(c) : QString();
                header.append(sanitizeInline(cell));
            }
            rows.append(header);
            i += 2;
            while (i < lines.size() && rows.size() <= kMaxTableRows) {
                const QString &row_line = lines.at(i);
                if (row_line.trimmed().isEmpty() || !row_line.contains(QLatin1Char('|')))
                    break;
                if (isTableSeparator(row_line)) {
                    ++i;
                    continue;
                }
                QStringList cells = splitTableRow(row_line);
                while (cells.size() < columns)
                    cells.append(QString());
                if (cells.size() > columns)
                    cells = cells.mid(0, columns);
                for (QString &cell : cells)
                    cell = sanitizeInline(cell);
                rows.append(cells);
                ++i;
            }
            Block block;
            block.type = QStringLiteral("table");
            block.rows = rowsToVariant(rows, columns);
            block.alignments = alignmentsToVariant(align_cells);
            block.columnWidths = columnWeights(rows, columns);
            blocks.append(block);
            continue;
        }

        // Setext headings take priority over a horizontal rule.
        if (!paragraph.isEmpty() &&
            (setext_one.match(line).hasMatch() || setext_two.match(line).hasMatch())) {
            const int level = setext_one.match(line).hasMatch() ? 1 : 2;
            Block block;
            block.type = QStringLiteral("heading");
            block.level = level;
            block.text = sanitizeInline(joinSoftLines(paragraph));
            paragraph.clear();
            blocks.append(block);
            ++i;
            continue;
        }

        const auto heading_match = atx_heading.match(line);
        if (heading_match.hasMatch()) {
            flush_paragraph();
            QString heading = heading_match.captured(2).trimmed();
            static const QRegularExpression trailing_hashes(
                QStringLiteral(R"([ \t]+#+[ \t]*$)"));
            heading.remove(trailing_hashes);
            Block block;
            block.type = QStringLiteral("heading");
            block.level = heading_match.captured(1).size();
            block.text = sanitizeInline(heading);
            blocks.append(block);
            ++i;
            continue;
        }

        if (horizontal_rule.match(line).hasMatch()) {
            flush_paragraph();
            Block block;
            block.type = QStringLiteral("hr");
            blocks.append(block);
            ++i;
            continue;
        }

        const auto quote_match = quote_re.match(line);
        if (quote_match.hasMatch()) {
            flush_paragraph();
            QStringList quote_lines;
            while (i < lines.size()) {
                const auto current = quote_re.match(lines.at(i));
                if (!current.hasMatch())
                    break;
                quote_lines.append(current.captured(1));
                ++i;
            }
            Block block;
            block.type = QStringLiteral("quote");
            block.text = sanitizeInline(quote_lines.join(QLatin1Char('\n')));
            blocks.append(block);
            continue;
        }

        const auto bullet_match = bullet_re.match(line);
        const auto ordered_match = ordered_re.match(line);
        if (bullet_match.hasMatch() || ordered_match.hasMatch()) {
            flush_paragraph();
            QString content;
            QString marker;
            int indent = 0;
            if (ordered_match.hasMatch()) {
                indent = qBound(0, leadingIndent(ordered_match.captured(1)) / 2, 6);
                marker = ordered_match.captured(2) + QStringLiteral(".");
                content = ordered_match.captured(3);
            } else {
                indent = qBound(0, leadingIndent(bullet_match.captured(1)) / 2, 6);
                content = bullet_match.captured(2);
            }
            int checked = -1;
            const auto task_match = task_re.match(content);
            if (task_match.hasMatch()) {
                checked = task_match.captured(1).compare(QStringLiteral(" "), Qt::CaseInsensitive) == 0
                              ? 0
                              : 1;
                content = task_match.captured(2);
            }
            Block block;
            block.type = marker.isEmpty() ? QStringLiteral("bullet") : QStringLiteral("ordered");
            block.text = sanitizeInline(content);
            block.marker = marker;
            block.indent = indent;
            block.checked = checked;
            blocks.append(block);
            ++i;
            continue;
        }

        paragraph.append(line);
        ++i;
    }

    flush_paragraph();
    return blocks;
}
