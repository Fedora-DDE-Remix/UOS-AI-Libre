#pragma once

#include <QString>

/// Helpers shared by the DDE compatibility service and AppController.
///
/// The DDE side sends two very different kinds of images: a path to a
/// short-lived screenshot file, or a base64 blob (UOS AI's quick-OCR callers
/// sometimes qCompress a PNG first). They also send documents by path. These
/// helpers normalise both forms before anything reaches aish, so the chat
/// layer only ever sees a stable local image path or bounded text.
namespace DdePayload {

/// Maximum accepted source image size before decoding.
constexpr qint64 kMaxImageBytes = 20 * 1024 * 1024;
/// Maximum accepted decoded pixel count (guards decompression bombs).
constexpr qint64 kMaxImagePixels = 50 * 1000 * 1000;
/// Maximum characters copied from a document into a prompt.
constexpr int kMaxDocumentChars = 48000;

/// Normalise `raw` (absolute path, file URL, data URL, base64 PNG/JPEG, or
/// qCompress'ed base64 PNG) into a stable PNG path in the application cache.
/// Returns an empty string and fills `error` on failure.
QString normalizeImage(const QString &raw, QString *error);

/// Read a text document or PDF into bounded UTF-8 text.
/// `truncated` is set when the result was clipped.
QString readDocument(const QString &path, int maxChars, bool *truncated,
                     QString *error);

/// Remove stale quick-action images from the cache.
void cleanupCache();

} // namespace DdePayload
