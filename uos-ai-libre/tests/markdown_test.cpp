#include "markdownmodel.h"

#include <QCoreApplication>
#include <QElapsedTimer>
#include <QMetaType>
#include <QTextStream>
#include <QVariantList>

namespace {

int failures = 0;

void check(bool condition, const char *what) {
    if (condition)
        return;
    QTextStream(stderr) << "FAIL: " << what << "\n";
    ++failures;
}

int role(const MarkdownModel &model, const char *name) {
    const auto roles = model.roleNames();
    for (auto it = roles.cbegin(); it != roles.cend(); ++it) {
        if (it.value() == name)
            return it.key();
    }
    return -1;
}

QString typeAt(const MarkdownModel &model, int row) {
    return model.data(model.index(row), role(model, "type")).toString();
}

QString textAt(const MarkdownModel &model, int row) {
    return model.data(model.index(row), role(model, "text")).toString();
}

} // namespace

int main(int argc, char **argv) {
    QCoreApplication app(argc, argv);

    {
        MarkdownModel model;
        model.setText(QStringLiteral(
            "# 标题\n\n"
            "第一行\n第二行\n\n"
            "- 项目一\n"
            "- [x] 已完成\n\n"
            "1. 第一\n"
            "2. 第二\n\n"
            "```cpp\nint x = 1;\n```\n\n"
            "| A | B |\n| --- | :---: |\n| 1 | 2 |\n\n"
            "> 引用内容\n\n"
            "---\n\n"
            "$$\nx^2\n$$\n"));

        check(model.rowCount() >= 10, "markdown produced blocks");
        check(typeAt(model, 0) == QStringLiteral("heading"), "heading type");
        check(textAt(model, 0) == QStringLiteral("标题"), "heading text");
        check(model.data(model.index(0), role(model, "level")).toInt() == 1,
              "heading level");
        check(textAt(model, 1) == QStringLiteral("第一行第二行"), "CJK soft join");
        check(typeAt(model, 2) == QStringLiteral("bullet"), "bullet type");
        check(typeAt(model, 3) == QStringLiteral("bullet"), "task type");
        check(model.data(model.index(3), role(model, "checked")).toInt() == 1,
              "task checked");
        check(typeAt(model, 4) == QStringLiteral("ordered"), "ordered type");
        check(model.data(model.index(4), role(model, "marker")).toString() ==
                  QStringLiteral("1."),
              "ordered marker");
        check(typeAt(model, 6) == QStringLiteral("code"), "code type");
        check(model.data(model.index(6), role(model, "lang")).toString() ==
                  QStringLiteral("cpp"),
              "code lang");
        check(typeAt(model, 7) == QStringLiteral("table"), "table type");
        const QVariantList rows =
            model.data(model.index(7), role(model, "rows")).toList();

        check(rows.size() == 2, "table rows");
        const QVariantList header = rows.value(0).toList();
        check(header.value(0).toString() == QStringLiteral("A"), "table header");
        check(typeAt(model, 8) == QStringLiteral("quote"), "quote type");
        check(typeAt(model, 9) == QStringLiteral("hr"), "hr type");
        check(typeAt(model, 10) == QStringLiteral("math"), "math type");
    }

    {
        MarkdownModel model;
        model.setText(QStringLiteral("first\nsecond"));
        check(textAt(model, 0) == QStringLiteral("first second"),
              "Latin soft join");
        model.setText(QStringLiteral("first\nsecond\n\n- item"));
        check(model.rowCount() == 2, "append block");
        check(typeAt(model, 1) == QStringLiteral("bullet"), "append bullet");
        model.setText(QStringLiteral("## changed"));
        check(model.rowCount() == 1, "shrink blocks");
        check(typeAt(model, 0) == QStringLiteral("heading"), "type changes");
        check(model.data(model.index(0), role(model, "level")).toInt() == 2,
              "heading level changes");
    }

    {
        MarkdownModel model;
        int changes = 0;
        QObject::connect(&model, &QAbstractItemModel::dataChanged,
                         [&changes](const QModelIndex &, const QModelIndex &,
                                    const QVector<int> &) { ++changes; });
        model.setText(QStringLiteral("hello"));
        model.setText(QStringLiteral("hello world"));
        check(changes > 0, "dataChanged emitted for streaming text");
        check(model.rowCount() == 1, "streaming single block");
    }

    {
        MarkdownModel model;
        model.setText(QString(300 * 1024 + 20, QLatin1Char('a')));
        check(model.truncated(), "truncation flag");
        check(model.rowCount() >= 1, "truncated blocks");
    }

    {
        MarkdownModel model;
        model.setText(QStringLiteral("hello <script>alert(1)</script>"));
        check(model.rowCount() == 1, "raw html paragraph");
        check(!textAt(model, 0).contains(QStringLiteral("<script>")),
              "raw html is escaped");
    }

    {
        MarkdownModel model;
        model.setText(QStringLiteral(
            "| 左 | 中 | 右 |\n"
            "| :--- | :---: | ---: |\n"
            "| a | b | c |\n"));
        const QVariantList align =
            model.data(model.index(0), role(model, "alignments")).toList();
        check(align.size() == 3, "table alignments");
        check(align.value(0).toInt() == 0 && align.value(1).toInt() == 1 &&
                  align.value(2).toInt() == 2,
              "table alignment values");
    }

    {
        QString document;
        for (int i = 0; i < 300; ++i) {
            document += QStringLiteral("## 第 %1 节\n\n").arg(i);
            document += QStringLiteral("这是一段用于基准测试的中文段落，包含 **粗体**、"
                                       "`行内代码` 和 [链接](https://example.com)。\n\n");
            document += QStringLiteral("| 指标 | 数值 | 说明 |\n"
                                       "| --- | ---: | --- |\n"
                                       "| 项目 | %1 | 基准测试行 |\n\n").arg(i);
            if (i % 5 == 0) {
                document += QStringLiteral("```cpp\nint value = %1;\n"
                                           "for (int i = 0; i < value; ++i) {}\n```\n\n").arg(i);
            }
        }
        check(document.size() > 32 * 1024, "benchmark document is large");
        MarkdownModel model;
        model.setText(document);
        check(model.rowCount() > 100, "benchmark parsed blocks");
        QElapsedTimer timer;
        timer.start();
        const int iterations = 20;
        for (int i = 0; i < iterations; ++i) {
            model.setText(document + QStringLiteral("\n\n尾部增量 %1").arg(i));
            model.setText(document);
        }
        const double average = double(timer.nsecsElapsed()) / 1e6 / (iterations * 2);
        QTextStream(stdout) << "parse_avg_ms=" << average
                            << " doc_bytes=" << document.size() << "\n";
        check(average < 100.0, "markdown parse stays below 100 ms");
    }

    if (failures == 0)
        QTextStream(stdout) << "MARKDOWN_TESTS_OK\n";
    return failures == 0 ? 0 : 1;
}
