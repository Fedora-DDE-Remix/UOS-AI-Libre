#include "dde/dde_payload.h"
#include "dde/dde_prompt.h"

#include <QBuffer>
#include <QColor>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QImageReader>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <iostream>

namespace {

int fail(const char *message) {
    std::cerr << "dde-payload-test: " << message << '\n';
    return 1;
}

bool imageReadable(const QString &path) {
    QImageReader reader(path);
    return !reader.read().isNull();
}

} // namespace

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);

    QTemporaryDir dir;
    if (!dir.isValid())
        return fail("temporary directory");

    QImage image(40, 24, QImage::Format_ARGB32);
    image.fill(QColor(210, 50, 40));
    const QString source = dir.filePath(QStringLiteral("source.png"));
    if (!image.save(source, "PNG"))
        return fail("write source image");

    QString error;
    const QString cached = DdePayload::normalizeImage(source, &error);
    if (cached.isEmpty() || !imageReadable(cached))
        return fail("normalize local path");
    if (error.size() != 0)
        return fail("local path error should be empty");
    if (!QFile::remove(source) || !imageReadable(cached))
        return fail("cached image must survive source deletion");

    QByteArray raw;
    QBuffer buffer(&raw);
    buffer.open(QIODevice::WriteOnly);
    image.save(&buffer, "PNG");
    const QString base64 =
        QString::fromLatin1(raw.toBase64(QByteArray::Base64Encoding));
    const QString fromBase64 = DdePayload::normalizeImage(base64, &error);
    if (fromBase64.isEmpty() || !imageReadable(fromBase64))
        return fail("normalize base64 image");

    const QString fromCompressed = DdePayload::normalizeImage(
        QString::fromLatin1(qCompress(raw, 9).toBase64()), &error);
    if (fromCompressed.isEmpty() || !imageReadable(fromCompressed))
        return fail("normalize qCompress base64 image");

    if (!DdePayload::normalizeImage(QStringLiteral("not-an-image"), &error).isEmpty()
        || error.isEmpty())
        return fail("invalid image should fail with an error");

    const QString text = dir.filePath(QStringLiteral("note.txt"));
    QFile file(text);
    if (!file.open(QIODevice::WriteOnly))
        return fail("write text file");
    file.write("0123456789abcdef");
    file.close();
    bool truncated = false;
    const QString document =
        DdePayload::readDocument(text, 9, &truncated, &error);
    if (document.isEmpty() || !truncated
        || !document.contains(QStringLiteral("内容已截断")))
        return fail("document truncation");

    if (DdePrompt::quickImagePrompt(1).isEmpty()
        || DdePrompt::quickImagePrompt(2).isEmpty()
        || DdePrompt::quickImagePrompt(3).isEmpty())
        return fail("quick prompts");
    const QString translation =
        DdePrompt::translationPrompt(QStringLiteral("hello world"));
    if (!translation.contains(QStringLiteral("中文")))
        return fail("translation target");
    const QString documentPrompt = DdePrompt::documentPrompt(
        QString(), QStringLiteral("总结"), text, document, true);
    if (!documentPrompt.contains(text)
        || !documentPrompt.contains(QStringLiteral("总结")))
        return fail("document prompt");

    const QString selection = QStringLiteral("followalong-selection");
    for (int type = 1; type <= 7; ++type) {
        const QString prompt = DdePrompt::followAlongPrompt(type, selection);
        if (prompt.trimmed().isEmpty() || !prompt.contains(selection))
            return fail("followalong prompt");
    }
    const QString customFollowAlong = DdePrompt::followAlongPrompt(
        -1, selection, QStringLiteral("Q: ${SELECTION}"));
    if (!customFollowAlong.contains(selection)
        || !customFollowAlong.startsWith(QStringLiteral("Q:")))
        return fail("custom followalong prompt");

    DdePayload::cleanupCache();
    std::cout << "dde-payload-test: ok\n";
    return 0;
}
