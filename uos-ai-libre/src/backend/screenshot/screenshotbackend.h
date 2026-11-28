#pragma once

#include <QImage>
#include <QString>

/// Platform backend for taking one still screenshot.
///
/// The public surface is always QImage; X11 (QScreen::grabWindow), Treeland
/// (treeland_capture_manager_v1) and any future portal backend are selected by
/// the factory and hidden from callers.
class ScreenshotBackend {
public:
    virtual ~ScreenshotBackend() = default;

    virtual QString name() const = 0;
    virtual bool available() const = 0;
    virtual bool capture(QImage *image, QString *error) = 0;
};
