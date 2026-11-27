#include "dde_payload.h"

#include <QBuffer>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QImage>
#include <QImageReader>
#include <QProcess>
#include <QRegularExpression>
#include <QSet>
#include <QStandardPaths>
#include <QUuid>
#include <QUrl>
#include <QtEndian>

#include <algorithm>

namespace {

QString cacheDirectory() {
    QString base =
        QStandardPaths::writableLocation(QStandardPaths::CacheLocation);
    if (base.trimmed().isEmpty())
        base = QDir::tempPath() + QStringLiteral("/uos-ai-libre");
    const QString dir = base + QStringLiteral("/dde-images");
    QDir().mkpath(dir);
    return dir;
}

QByteArray decodeBase64(QString raw) {
    raw.remove(QRegularExpression(QStringLiteral("\\s")));
    if (raw.isEmpty())
        return {};
    const QByteArray encoded = raw.toLatin1();
    return QByteArray::fromBase64(
        encoded, QByteArray::Base64Encoding |
                     QByteArray::AbortOnBase64DecodingErrors);
}

bool readImage(const QByteArray &bytes, QImage *image, QString *error) {
    if (bytes.isEmpty()) {
        if (error)
            *error = QStringLiteral("图片数据为空");
        return false;
    }
    if (bytes.size() > DdePayload::kMaxImageBytes) {
        if (error)
            *error = QStringLiteral("图片数据超过 20MB 限制");
        return false;
    }
    QBuffer buffer;
    buffer.setData(bytes);
    if (!buffer.open(QIODevice::ReadOnly)) {
        if (error)
            *error = QStringLiteral("无法读取图片数据");
        return false;
    }
    QImageReader reader(&buffer);
    reader.setDecideFormatFromContent(true);
    const QSize size = reader.size();
    if (size.isValid()
        && static_cast<qint64>(size.width()) * size.height()
               > DdePayload::kMaxImagePixels) {
        if (error)
            *error = QStringLiteral("图片像素尺寸过大");
        return false;
    }
    const QImage decoded = reader.read();
    if (decoded.isNull()) {
        if (error)
            *error = QStringLiteral("无法识别图片格式：%1")
                         .arg(reader.errorString());
        return false;
    }
    if (static_cast<qint64>(decoded.width()) * decoded.height()
        > DdePayload::kMaxImagePixels) {
        if (error)
            *error = QStringLiteral("图片像素尺寸过大");
        return false;
    }
    *image = decoded;
    return true;
}

QString saveCachedImage(const QImage &image, QString *error) {
    const QString dir = cacheDirectory();
    if (dir.isEmpty() || !QDir(dir).exists()) {
        if (error)
            *error = QStringLiteral("无法创建图片缓存目录");
        return {};
    }
    const QString path =
        dir + QStringLiteral("/quick-ai-%1.png")
                  .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    if (!image.save(path, "PNG")) {
        if (error)
            *error = QStringLiteral("无法保存图片到缓存");
        return {};
    }
    DdePayload::cleanupCache();
    return path;
}

QString fromRawBytes(const QByteArray &bytes, QString *error) {
    QImage image;
    if (!readImage(bytes, &image, error))
        return {};
    return saveCachedImage(image, error);
}

} // namespace

namespace DdePayload {

QString normalizeImage(const QString &raw, QString *error) {
    if (error)
        error->clear();
    const QString trimmed = raw.trimmed();
    if (trimmed.isEmpty()) {
        if (error)
            *error = QStringLiteral("图片路径为空");
        return {};
    }

    // A normal local path. Copy it into the cache so the caller (for example
    // deepin-screen-recorder) can delete its short-lived file immediately.
    QString path = trimmed;
    if (path.startsWith(QStringLiteral("file://"), Qt::CaseInsensitive))
        path = QUrl(path).toLocalFile();
    if (!path.contains(QLatin1Char('\n')) && QFileInfo(path).isFile()) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            if (error)
                *error = QStringLiteral("无法读取图片：%1").arg(path);
            return {};
        }
        if (file.size() > kMaxImageBytes) {
            if (error)
                *error = QStringLiteral("图片超过 20MB 限制");
            return {};
        }
        return fromRawBytes(file.readAll(), error);
    }

    // data:image/png;base64,...
    QString encoded = trimmed;
    if (encoded.startsWith(QStringLiteral("data:"), Qt::CaseInsensitive)) {
        const qsizetype comma = encoded.indexOf(QLatin1Char(','));
        if (comma < 0) {
            if (error)
                *error = QStringLiteral("无效的 data URL 图片");
            return {};
        }
        encoded = encoded.mid(comma + 1);
    }

    QByteArray bytes = decodeBase64(encoded);
    if (bytes.size() > kMaxImageBytes) {
        if (error)
            *error = QStringLiteral("图片数据超过 20MB 限制");
        return {};
    }
    if (!bytes.isEmpty()) {
        QImage image;
        if (readImage(bytes, &image, nullptr))
            return saveCachedImage(image, error);

        // UOS AI's launchAiQuickOCRWithImage() sends qCompress(PNG) as base64.
        // qCompress stores the uncompressed size in the first four bytes; check
        // it before allocating so a small malicious payload cannot expand into
        // an unbounded buffer.
        if (bytes.size() >= 4) {
            const quint32 expected = qFromBigEndian<quint32>(
                reinterpret_cast<const uchar *>(bytes.constData()));
            if (expected <= static_cast<quint32>(kMaxImageBytes)) {
                const QByteArray uncompressed = qUncompress(bytes);
                if (!uncompressed.isEmpty()) {
                    if (error)
                        error->clear();
                    return fromRawBytes(uncompressed, error);
                }
            }
        }
    }

    if (error)
        *error = QStringLiteral("不支持的图片输入（需要文件路径或 base64 图片）");
    return {};
}

QString readDocument(const QString &path, int maxChars, bool *truncated,
                     QString *error) {
    if (error)
        error->clear();
    if (truncated)
        *truncated = false;
    if (maxChars <= 0)
        maxChars = kMaxDocumentChars;

    const QFileInfo info(path);
    if (!info.isFile()) {
        if (error)
            *error = QStringLiteral("文件不存在或不是普通文件");
        return {};
    }
    if (info.size() > 128LL * 1024 * 1024) {
        if (error)
            *error = QStringLiteral("文件超过 128MB 限制");
        return {};
    }

    const QString suffix = info.suffix().toLower();
    const QSet<QString> textSuffixes = {
        QStringLiteral("txt"),   QStringLiteral("md"),
        QStringLiteral("markdown"), QStringLiteral("json"),
        QStringLiteral("yaml"),  QStringLiteral("yml"),
        QStringLiteral("xml"),   QStringLiteral("csv"),
        QStringLiteral("log"),   QStringLiteral("c"),
        QStringLiteral("cc"),    QStringLiteral("cpp"),
        QStringLiteral("cxx"),   QStringLiteral("h"),
        QStringLiteral("hpp"),   QStringLiteral("py"),
        QStringLiteral("js"),    QStringLiteral("ts"),
        QStringLiteral("jsx"),   QStringLiteral("tsx"),
        QStringLiteral("html"),  QStringLiteral("htm"),
        QStringLiteral("css"),   QStringLiteral("scss"),
        QStringLiteral("sh"),    QStringLiteral("bash"),
        QStringLiteral("zsh"),   QStringLiteral("go"),
        QStringLiteral("rs"),    QStringLiteral("java"),
        QStringLiteral("kt"),    QStringLiteral("kts"),
        QStringLiteral("rb"),    QStringLiteral("php"),
        QStringLiteral("sql"),   QStringLiteral("toml"),
        QStringLiteral("ini"),   QStringLiteral("conf"),
    };

    QString text;
    if (textSuffixes.contains(suffix)) {
        if (info.size() > 16LL * 1024 * 1024) {
            if (error)
                *error = QStringLiteral("文本文件超过 16MB 限制");
            return {};
        }
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            if (error)
                *error = QStringLiteral("无法读取文件");
            return {};
        }
        const QByteArray head = file.read(4096);
        if (head.contains('\0')) {
            if (error)
                *error = QStringLiteral("文件看起来是二进制格式");
            return {};
        }
        file.seek(0);
        text = QString::fromUtf8(file.readAll());
    } else if (suffix == QStringLiteral("pdf")) {
        const QString program =
            QStandardPaths::findExecutable(QStringLiteral("pdftotext"));
        if (program.isEmpty()) {
            if (error)
                *error = QStringLiteral("系统没有安装 pdftotext，无法读取 PDF");
            return {};
        }
        QProcess process;
        process.setProgram(program);
        process.setArguments({QStringLiteral("-layout"), path, QStringLiteral("-")});
        process.start();
        if (!process.waitForStarted(3000)) {
            if (error)
                *error = QStringLiteral("无法启动 pdftotext");
            return {};
        }
        if (!process.waitForFinished(15000)) {
            process.kill();
            process.waitForFinished(1000);
            if (error)
                *error = QStringLiteral("PDF 文本提取超时");
            return {};
        }
        if (process.exitStatus() != QProcess::NormalExit
            || process.exitCode() != 0) {
            if (error)
                *error = QStringLiteral("PDF 文本提取失败：%1")
                             .arg(QString::fromLocal8Bit(process.readAllStandardError())
                                      .trimmed());
            return {};
        }
        text = QString::fromUtf8(process.readAllStandardOutput());
    } else {
        if (error)
            *error = QStringLiteral("暂不支持自动提取 .%1 文件，请改为文本或 PDF")
                         .arg(suffix.isEmpty() ? QStringLiteral("未知") : suffix);
        return {};
    }

    if (text.size() > maxChars) {
        text.truncate(maxChars);
        if (truncated)
            *truncated = true;
        text += QStringLiteral("\n\n[内容已截断]");
    }
    return text;
}

void cleanupCache() {
    const QString dir = cacheDirectory();
    if (!dir.isEmpty()) {
        QDir directory(dir);
        const QDateTime cutoff =
            QDateTime::currentDateTime().addDays(-1);
        const QFileInfoList entries =
            directory.entryInfoList({QStringLiteral("quick-ai-*.png")},
                                    QDir::Files, QDir::Time);
        for (int i = 0; i < entries.size(); ++i) {
            if (i >= 32 || entries.at(i).lastModified() < cutoff)
                QFile::remove(entries.at(i).absoluteFilePath());
        }
    }
}

} // namespace DdePayload
