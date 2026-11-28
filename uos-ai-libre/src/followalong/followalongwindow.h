// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <QObject>
#include <QPointer>
#include <QPoint>
#include <QRect>
#include <QSize>
#include <QVariantList>

class QQmlComponent;
class QQmlEngine;
class QQuickWindow;

/// Owns the small AI FollowAlong toolbar surface.
class FollowAlongWindowHost final : public QObject
{
    Q_OBJECT

public:
    explicit FollowAlongWindowHost(QQmlEngine *engine,
                                   QObject *parent = nullptr);
    ~FollowAlongWindowHost() override;

    void show(const QString &text, const QPoint &globalPos,
              const QVariantList &actions);
    void hide();
    bool visible() const;
    QRect geometry() const;
    bool cursorInside() const;
    bool cursorPositionAvailable() const;
    void setSize(const QSize &size);
    void setExpanded(bool expanded);
    void activateForInput();

Q_SIGNALS:
    void actionTriggered(int type, bool custom, const QString &text);
    void dismissed();

private slots:
    void onActionTriggered(int type, bool custom);
    void onDismissed();

private:
    QQmlComponent *component();
    void ensureWindow();
    void positionX11();
    void positionX11(const QSize &size);
    QPoint positionForSize(const QSize &size) const;
    void destroyWindow();

    QQmlEngine *m_engine = nullptr;
    QQmlComponent *m_component = nullptr;
    QPointer<QQuickWindow> m_window;
    QObject *m_overlay = nullptr;
    QPoint m_anchor;
    QString m_text;
};
