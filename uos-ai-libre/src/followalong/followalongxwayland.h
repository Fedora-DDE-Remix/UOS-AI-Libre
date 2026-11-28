// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QPoint>

#include <optional>

/// Global pointer position through the XWayland root window.
///
/// Native Wayland clients cannot query the seat pointer. DDE always runs
/// XWayland so X11 applications keep working; the root pointer tracks the real
/// seat. Only trust it when the XWayland root and Qt's virtual desktop describe
/// the same coordinate space.
std::optional<QPoint> followAlongXWaylandPointerPosition();
bool followAlongXWaylandCoordinateSpaceReliable();
