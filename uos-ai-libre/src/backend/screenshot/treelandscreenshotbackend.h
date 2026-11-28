#pragma once

#include "screenshotbackend.h"

/// One-shot screenshot through Treeland's capture protocol.
///
/// Treeland exposes treeland_capture_manager_v1 (deprecated but still the
/// native screenshot protocol). Its context performs compositor-side source
/// selection and returns one shm-backed frame.
class TreelandScreenshotBackend final : public ScreenshotBackend {
public:
    QString name() const override;
    bool available() const override;
    bool capture(QImage *image, QString *error) override;
};
