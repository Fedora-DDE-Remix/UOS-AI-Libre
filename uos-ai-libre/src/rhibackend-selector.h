#pragma once

#include <QtGui/QGuiApplication>
#include <QtQuick/QSGRendererInterface>

namespace RhiBackendSelector {

// Returns Unknown when an explicit Qt backend environment variable should be
// respected as-is. Otherwise returns the backend that should be passed to
// QQuickWindow::setGraphicsApi().
QSGRendererInterface::GraphicsApi select(QString *detail = nullptr);

} // namespace RhiBackendSelector
