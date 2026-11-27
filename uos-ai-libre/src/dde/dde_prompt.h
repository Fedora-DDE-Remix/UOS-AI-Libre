#pragma once

#include <QString>

/// Prompt construction for DDE-originated actions.
///
/// Keeping these strings in one place makes the compatibility layer
/// independent from the chat UI and lets tests verify the exact intent sent to
/// the user-selected cloud model.
namespace DdePrompt {

/// mode 1 = explain, 2 = summarize, 3 = translate, 4 = ask about image.
QString quickImagePrompt(int mode);

QString documentPrompt(const QString &question, const QString &defaultPrompt,
                       const QString &path, const QString &text,
                       bool truncated);

QString translationPrompt(const QString &text);

} // namespace DdePrompt
