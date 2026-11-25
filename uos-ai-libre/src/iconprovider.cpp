#include "iconprovider.h"

#include <QColor>
#include <QFile>
#include <QPainter>
#include <QRegularExpression>
#include <QSvgRenderer>
#include <QUrlQuery>

AppIconProvider::AppIconProvider()
    : QQuickImageProvider(QQuickImageProvider::Image) {}

QImage AppIconProvider::requestImage(const QString &id, QSize *size,
                                     const QSize &requestedSize) {
    QString path = id;
    QString query;
    const int query_at = path.indexOf(QLatin1Char('?'));
    if (query_at >= 0) {
        query = path.mid(query_at + 1);
        path = path.left(query_at);
    }

    static const QRegularExpression valid_name(
        QStringLiteral("^[a-z0-9-]{1,40}$"));
    if (!valid_name.match(path).hasMatch()) {
        if (size)
            *size = QSize();
        return {};
    }

    const QString svg_path = QStringLiteral(":/icons/%1.svg").arg(path);
    if (!QFile::exists(svg_path)) {
        if (size)
            *size = QSize();
        return {};
    }

    QSize target = requestedSize;
    if (!target.isValid() || target.width() <= 0 || target.height() <= 0)
        target = QSize(24, 24);
    target.setWidth(qBound(8, target.width(), 256));
    target.setHeight(qBound(8, target.height(), 256));

    QSvgRenderer renderer(svg_path);
    if (!renderer.isValid()) {
        if (size)
            *size = QSize();
        return {};
    }

    QImage image(target, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    {
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing, true);
        renderer.render(&painter);
    }

    QColor tint(QStringLiteral("#333333"));
    if (!query.isEmpty()) {
        const QUrlQuery params(query);
        const QString color_name = params.queryItemValue(QStringLiteral("color"));
        if (QColor::isValidColorName(color_name))
            tint = QColor(color_name);
    }

    if (tint.isValid() && tint.alpha() > 0) {
        QPainter painter(&image);
        painter.setCompositionMode(QPainter::CompositionMode_SourceIn);
        painter.fillRect(image.rect(), tint);
    }

    if (size)
        *size = target;
    return image;
}
