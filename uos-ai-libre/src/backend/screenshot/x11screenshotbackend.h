#pragma once

#include "screenshotbackend.h"

/// X11 screenshot backend based on QScreen::grabWindow().
class X11ScreenshotBackend final : public ScreenshotBackend {
public:
    QString name() const override;
    bool available() const override;
    bool capture(QImage *image, QString *error) override;
};
