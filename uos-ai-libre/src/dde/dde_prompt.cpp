#include "dde_prompt.h"

#include <QRegularExpression>

namespace {

bool containsCjk(const QString &text) {
    static const QRegularExpression cjk(
        QStringLiteral("[\\x{3400}-\\x{4DBF}\\x{4E00}-\\x{9FFF}]"));
    return cjk.match(text).hasMatch();
}

} // namespace

namespace DdePrompt {

QString quickImagePrompt(int mode) {
    switch (mode) {
    case 1:
        return QStringLiteral(
            "请解释这张截图的内容。说明它展示了什么、关键信息和用户可能关心的问题。"
            "直接给出结果，不要描述你正在分析。");
    case 2:
        return QStringLiteral(
            "请总结这张截图的主要内容，提炼关键信息和结论。"
            "直接给出摘要，不要描述你正在分析。");
    case 3:
        return QStringLiteral(
            "请翻译这张截图中的文字。如果原文主要是中文，翻译成英文；"
            "否则翻译成中文。只输出翻译结果，保留必要的结构。");
    case 4:
        return QStringLiteral(
            "请阅读这张截图并回答我的问题。如果截图里有文字，请先理解文字内容。");
    default:
        return {};
    }
}

QString documentPrompt(const QString &question, const QString &defaultPrompt,
                       const QString &path, const QString &text,
                       bool truncated) {
    QString prompt = question.trimmed();
    if (prompt.isEmpty())
        prompt = defaultPrompt.trimmed();
    if (prompt.isEmpty())
        prompt = QStringLiteral("请分析这个文件，提炼关键信息并给出结论。");

    QString result = prompt;
    if (!path.trimmed().isEmpty()) {
        result += QStringLiteral("\n\n");
        result += QStringLiteral("文件：%1\n").arg(path);
        if (!text.trimmed().isEmpty()) {
            result += QStringLiteral("以下是文件内容（仅供分析）：\n\n");
            result += text;
            if (truncated)
                result += QStringLiteral("\n\n[文件内容已截断]");
        } else {
            result += QStringLiteral(
                "我无法在本地提取该文件的文本；请使用可用的文件读取工具读取该路径后回答。");
        }
    }
    return result;
}

QString translationPrompt(const QString &text) {
    const QString target = containsCjk(text) ? QStringLiteral("英文")
                                             : QStringLiteral("中文");
    return QStringLiteral(
               "请把下面的文本翻译成%1。只输出翻译结果，不要解释，不要添加原文。\n\n"
               "---\n%2\n---")
        .arg(target, text);
}

QString followAlongPrompt(int type, const QString &text,
                          const QString &customPrompt) {
    if (!customPrompt.trimmed().isEmpty()) {
        QString prompt = customPrompt;
        prompt.replace(QStringLiteral("${SELECTION}"), text);
        if (!prompt.contains(text))
            prompt += QStringLiteral("\n\n---\n") + text;
        return prompt;
    }

    switch (type) {
    case 1:
        return QStringLiteral(
                   "请解释下面的选中文本。说明它的含义、背景和关键点，"
                   "直接给出结果。\n\n---\n%1\n---")
            .arg(text);
    case 2:
        return QStringLiteral(
                   "请总结下面的选中文本，提炼关键信息和结论，"
                   "直接给出摘要。\n\n---\n%1\n---")
            .arg(text);
    case 3:
        return translationPrompt(text);
    case 4:
        return QStringLiteral(
                   "请接着下面的选中文本继续写作，保持原有语气和风格，"
                   "只输出续写内容。\n\n---\n%1\n---")
            .arg(text);
    case 5:
        return QStringLiteral(
                   "请扩写下面的选中文本，补充必要的细节和论证，"
                   "保持原意，直接给出扩写结果。\n\n---\n%1\n---")
            .arg(text);
    case 6:
        return QStringLiteral(
                   "请检查并纠正下面选中文本中的错别字、语法和表达问题，"
                   "只输出修改后的文本。\n\n---\n%1\n---")
            .arg(text);
    case 7:
        return QStringLiteral(
                   "请润色下面的选中文本，使表达更自然、专业、简洁，"
                   "只输出润色后的文本。\n\n---\n%1\n---")
            .arg(text);
    default:
        return QStringLiteral(
                   "请处理下面的选中文本，直接给出结果。\n\n---\n%1\n---")
            .arg(text);
    }
}

} // namespace DdePrompt
