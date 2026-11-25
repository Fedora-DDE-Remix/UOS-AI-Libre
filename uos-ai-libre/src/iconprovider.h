#pragma once

#include <QQuickImageProvider>
#include <QSize>

/// Serves bundled monochrome SVG icons tinted at request time.
///
/// QML asks for `image://appicon/<name>?color=%23RRGGBB`. The provider only
/// accepts a small name whitelist and never touches arbitrary paths, so an
/// untrusted chat message cannot turn an icon URL into a file read.
class AppIconProvider final : public QQuickImageProvider {
public:
    AppIconProvider();

    QImage requestImage(const QString &id, QSize *size,
                        const QSize &requestedSize) override;
};
