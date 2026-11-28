#include "screenshotservice.h"

#include "screenshotbackend.h"
#include "treelandscreenshotbackend.h"
#include "x11screenshotbackend.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QStandardPaths>

#include <memory>

namespace {

class UnavailableScreenshotBackend final : public ScreenshotBackend {
public:
    QString name() const override { return QStringLiteral("unavailable"); }
    bool available() const override { return false; }
    bool capture(QImage *image, QString *error) override {
        Q_UNUSED(image)
        if (error)
            *error = QStringLiteral("当前桌面环境没有可用的截图后端");
        return false;
    }
};

std::unique_ptr<ScreenshotBackend> createPlatformBackend() {
    const QString platform = QGuiApplication::platformName().toLower();
    if (platform.contains(QStringLiteral("wayland"))
        || platform.contains(QStringLiteral("treeland"))) {
        auto treeland = std::make_unique<TreelandScreenshotBackend>();
        if (treeland->available())
            return treeland;
        return std::make_unique<UnavailableScreenshotBackend>();
    }

    auto x11 = std::make_unique<X11ScreenshotBackend>();
    if (x11->available())
        return x11;
    return std::make_unique<UnavailableScreenshotBackend>();
}

QString screenshotDirectory() {
    return QStandardPaths::writableLocation(QStandardPaths::TempLocation)
           + QStringLiteral("/uos-ai-libre");
}

} // namespace

ScreenshotService::ScreenshotService() {
    // Bind the Treeland capture manager before the first user action. The
    // Wayland registry round-trip is asynchronous; doing it in the constructor
    // avoids a first-screenshot race where the manager is not active yet.
    ensureBackend();
}
ScreenshotService::~ScreenshotService() = default;

void ScreenshotService::ensureBackend() {
    if (m_backend && m_backend->available())
        return;
    m_backend = createPlatformBackend();
}

QString ScreenshotService::backendName() const {
    return m_backend ? m_backend->name() : QStringLiteral("uninitialized");
}

QString ScreenshotService::capture(QString *error) {
    if (error)
        error->clear();
    ensureBackend();
    if (!m_backend)
        return {};

    QImage image;
    QString capture_error;
    if (!m_backend->capture(&image, &capture_error)) {
        if (error)
            *error = capture_error.isEmpty() ? QStringLiteral("截图失败")
                                             : capture_error;
        return {};
    }
    if (image.isNull()) {
        if (error)
            *error = QStringLiteral("截图结果为空");
        return {};
    }

    const QString base = screenshotDirectory();
    QDir directory(base);
    if (!directory.exists() && !QDir().mkpath(base)) {
        if (error)
            *error = QStringLiteral("无法创建截图临时目录");
        return {};
    }
    QFile::setPermissions(base, QFileDevice::ReadOwner
                                   | QFileDevice::WriteOwner
                                   | QFileDevice::ExeOwner);
    const QString path =
        base + QStringLiteral("/screenshot-%1.png")
                   .arg(QDateTime::currentMSecsSinceEpoch());
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = QStringLiteral("无法保存截图");
        return {};
    }
    file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner);
    if (!image.save(&file, "PNG")) {
        file.close();
        QFile::remove(path);
        if (error)
            *error = QStringLiteral("无法编码截图");
        return {};
    }
    file.close();
    return path;
}

void ScreenshotService::cleanup() {
    QDir directory(screenshotDirectory());
    if (!directory.exists())
        return;
    const QDateTime cutoff = QDateTime::currentDateTime().addDays(-1);
    const QFileInfoList files = directory.entryInfoList(
        {QStringLiteral("screenshot-*.png")}, QDir::Files, QDir::Time);
    for (int i = 0; i < files.size(); ++i) {
        if (i >= 8 || files.at(i).lastModified() < cutoff)
            QFile::remove(files.at(i).filePath());
    }
}
