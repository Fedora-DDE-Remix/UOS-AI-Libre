// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>

class QWindow;

/// Attaches a Qt Quick surface to Treeland's private DDE shell overlay role.
///
/// This is the same role UOS AI 3.3 uses for its FollowAlong toolbar. The
/// compositor owns the position through set_auto_placement(), which is why
/// Wayland clients cannot use QCursor::pos() here.
class TreelandOverlay final : public QObject
{
    Q_OBJECT

public:
    static TreelandOverlay *attach(QWindow *window);
    ~TreelandOverlay() override;

    /// Let the compositor give this overlay keyboard focus (needed for the
    /// Ask AI field). The toolbar otherwise stays click-through for focus.
    void setKeyboardFocus(bool accepted);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    explicit TreelandOverlay(QWindow *window);
    void attachSurface();

    QWindow *m_window = nullptr;
    void *m_surface = nullptr;
};
