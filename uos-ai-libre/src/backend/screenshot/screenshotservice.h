#pragma once

#include <QString>

#include <memory>

class ScreenshotBackend;

/// Platform-neutral screenshot entry point.
///
/// Callers never test the platform: the service selects the X11, Treeland or
/// unavailable backend once and keeps that policy in one place.
class ScreenshotService {
public:
    ScreenshotService();
    ~ScreenshotService();

    ScreenshotService(const ScreenshotService &) = delete;
    ScreenshotService &operator=(const ScreenshotService &) = delete;

    /// Captures and saves a PNG; returns its path, or an empty string on error.
    QString capture(QString *error = nullptr);

    QString backendName() const;

    /// Removes old screenshots from the shared temporary directory.
    static void cleanup();

private:
    void ensureBackend();

    std::unique_ptr<ScreenshotBackend> m_backend;
};
