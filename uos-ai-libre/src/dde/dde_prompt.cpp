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

} // namespace DdePrompt
