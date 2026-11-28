#include "x11screenshotbackend.h"

#include <QGuiApplication>
#include <QPixmap>
#include <QScreen>

QString X11ScreenshotBackend::name() const {
    return QStringLiteral("x11");
}

bool X11ScreenshotBackend::available() const {
    const QString platform = QGuiApplication::platformName().toLower();
    return platform.contains(QStringLiteral("xcb"))
           || platform.contains(QStringLiteral("x11"));
}

bool X11ScreenshotBackend::capture(QImage *image, QString *error) {
    if (!image)
        return false;
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) {
        if (error)
            *error = QStringLiteral("无法访问屏幕");
        return false;
    }
    const QPixmap pixmap = screen->grabWindow(0);
    if (pixmap.isNull()) {
        if (error)
            *error = QStringLiteral(
                "屏幕截图失败（当前桌面环境可能不允许截图）");
        return false;
    }
    *image = pixmap.toImage();
    return !image->isNull();
}
