// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "followalongservice.h"

#include "followalongmonitor.h"
#include "followalongwindow.h"

#include "appcontroller.h"

#include <QCursor>
#include <QDateTime>
#include <QDesktopServices>
#include <QSettings>
#include <QUrl>
#include <QUrlQuery>
#include <QVariantMap>
#include <QDebug>

namespace {

QSettings settings()
{
    return QSettings(QSettings::IniFormat, QSettings::UserScope,
                     QStringLiteral("UOS-AI-Libre"),
                     QStringLiteral("followalong"));
}

QVariantMap action(const QString &name, const QString &icon, int type,
                   bool custom = false)
{
    QVariantMap item;
    item.insert(QStringLiteral("name"), name);
    item.insert(QStringLiteral("icon"), icon);
    item.insert(QStringLiteral("type"), type);
    item.insert(QStringLiteral("custom"), custom);
    return item;
}

} // namespace

FollowAlongService::FollowAlongService(QQmlEngine *engine, AppController *app,
                                       QObject *parent)
    : QObject(parent)
    , m_app(app)
    , m_window(std::make_unique<FollowAlongWindowHost>(engine))
{
    QSettings config = settings();
    m_enabled = config.value(QStringLiteral("enabled"), true).toBool();

    m_actions = {
        action(QStringLiteral("搜索"), QStringLiteral("search"), 0),
        action(QStringLiteral("解释"), QStringLiteral("help"), 1),
        action(QStringLiteral("总结"), QStringLiteral("report"), 2),
        action(QStringLiteral("翻译"), QStringLiteral("globe"), 3),
    };
    m_moreActions = {
        action(QStringLiteral("续写"), QStringLiteral("edit"), 4),
        action(QStringLiteral("扩写"), QStringLiteral("expand"), 5),
        action(QStringLiteral("纠错"), QStringLiteral("check"), 6),
        action(QStringLiteral("润色"), QStringLiteral("skill"), 7),
        action(QStringLiteral("加入知识库"), QStringLiteral("knowledge"), 8),
    };

    connect(m_window.get(), &FollowAlongWindowHost::actionTriggered, this,
            &FollowAlongService::onActionTriggered);
    connect(m_window.get(), &FollowAlongWindowHost::dismissed, this,
            [this]() {
                m_window->hide();
                if (m_monitor)
                    m_monitor->resetSelectionState();
            });

    // Wayland data-control events are dispatched from inside libwayland.
    // Never destroy/create a QWindow synchronously from that callback.
    m_showTimer.setSingleShot(true);
    m_showTimer.setInterval(0);
    connect(&m_showTimer, &QTimer::timeout, this,
            &FollowAlongService::showPending);
}

FollowAlongService::~FollowAlongService()
{
    if (m_monitor)
        m_monitor->stop();
}

bool FollowAlongService::enabled() const { return m_enabled; }

QVariantList FollowAlongService::actions() const { return m_actions; }

QVariantList FollowAlongService::moreActions() const { return m_moreActions; }

void FollowAlongService::ask(const QString &question)
{
    qInfo() << "FollowAlong: ask" << question << "selection"
            << m_pendingText.size();
    if (question.trimmed().isEmpty() || m_pendingText.trimmed().isEmpty())
        return;
    m_window->hide();
    if (m_monitor)
        m_monitor->resetSelectionState();
    if (m_app && m_app->startFollowAlongAction(
            -1, m_pendingText, true,
            question + QStringLiteral("\n\n选中内容：${SELECTION}"))) {
        Q_EMIT showChatRequested();
    }
}

void FollowAlongService::openSettings()
{
    m_window->hide();
    if (m_monitor)
        m_monitor->resetSelectionState();
    Q_EMIT settingsRequested();
}

void FollowAlongService::showPreview()
{
    m_pendingText = QStringLiteral("这是一段用于验证 AI 随航工具栏的选中文本。");
    m_pendingPos = QCursor::pos();
    const QString anchor =
        qEnvironmentVariable("UOS_AI_LIBRE_FOLLOWALONG_PREVIEW_ANCHOR");
    if (!anchor.isEmpty()) {
        const QStringList parts = anchor.split(QLatin1Char(','));
        if (parts.size() == 2)
            m_pendingPos = QPoint(parts.at(0).trimmed().toInt(),
                                  parts.at(1).trimmed().toInt());
    }
    const int preview =
        qEnvironmentVariableIntValue("UOS_AI_LIBRE_FOLLOWALONG_PREVIEW");
    const bool expanded = preview == 2 || preview == 4;
    m_window->show(m_pendingText, m_pendingPos, m_actions, expanded);
    if (preview == 3) {
        // Exercise the same icon -> expanded transition as a real click.
        QTimer::singleShot(500, m_window.get(),
                           [this]() { m_window->setExpanded(true); });
    } else if (preview == 4) {
        QTimer::singleShot(700, m_window.get(),
                           [this]() { m_window->openMoreMenu(); });
    } else if (preview == 5) {
        // Exercise hide() with the real RHI backend; this catches Vulkan
        // swapchain teardown regressions without pointer injection.
        QTimer::singleShot(800, m_window.get(),
                           [this]() { m_window->hide(); });
    } else if (preview == 6) {
        // Hide, re-show and hide again: the swapchain released on hide must
        // be recreated correctly when the toolbar is shown again.
        QTimer::singleShot(800, m_window.get(),
                           [this]() { m_window->hide(); });
        QTimer::singleShot(1600, m_window.get(), [this]() {
            m_window->show(m_pendingText, m_pendingPos, m_actions);
        });
        QTimer::singleShot(2400, m_window.get(),
                           [this]() { m_window->hide(); });
    }
}

void FollowAlongService::activateToolbar()
{
    m_window->activateForInput();
}

bool FollowAlongService::cursorPositionAvailable() const
{
    return m_window->cursorPositionAvailable();
}

int FollowAlongService::maxToolbarWidth() const
{
    return m_window->maxToolbarWidth();
}

void FollowAlongService::setToolbarSize(int width, int height)
{
    m_window->setSize(QSize(width, height));
}

void FollowAlongService::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    QSettings config = settings();
    config.setValue(QStringLiteral("enabled"), enabled);
    if (!enabled)
        m_window->hide();
    Q_EMIT enabledChanged();
}

void FollowAlongService::start()
{
    if (m_started)
        return;
    m_started = true;

    m_monitor.reset(FollowAlongMonitor::create(this));
    connect(m_monitor.get(), &FollowAlongMonitor::selectionChanged, this,
            &FollowAlongService::onSelectionChanged);
    connect(m_monitor.get(), &FollowAlongMonitor::selectionCleared, this,
            &FollowAlongService::onSelectionCleared);
    connect(m_monitor.get(), &FollowAlongMonitor::pointerPressed, this,
            &FollowAlongService::onGlobalPointerPressed);
    connect(m_monitor.get(), &FollowAlongMonitor::monitorError, this,
            [](const QString &message) {
                qWarning().noquote() << "FollowAlong monitor:" << message;
            });

    QString error;
    if (!m_monitor->start(&error))
        qWarning().noquote() << "FollowAlong unavailable:" << error;
}

void FollowAlongService::onSelectionChanged(const QString &text,
                                            const QPoint &globalPos)
{
    if (!m_enabled || text.trimmed().isEmpty())
        return;
    // Clicks on the toolbar itself must never move or re-show it. This also
    // covers the Ask AI field and popup menus, whose release coordinates can
    // be inside (or just outside) the pill.
    if (m_window && m_window->visible()
        && m_window->cursorPositionAvailable()
        && m_window->geometry().contains(globalPos)) {
        return;
    }
    m_pendingText = text;
    m_pendingPos = globalPos;
    m_showTimer.start();
}

void FollowAlongService::onGlobalPointerPressed(const QPoint &globalPos)
{
    if (!m_window || !m_window->visible())
        return;
    // Menu item clicks are outside the toolbar rectangle. Let the popup
    // finish handling them before deciding whether the window should close.
    if (m_window->property("menuOpen").toBool())
        return;

    QPoint pressPos = globalPos;
    bool exactPosition = m_window->cursorPositionAvailable();
    if (!exactPosition && m_window->hasGlobalCursorPosition()) {
        pressPos = m_window->globalCursorPosition();
        exactPosition = true;
    }
    if (exactPosition) {
        if (m_window->geometry().contains(pressPos))
            return;
        qInfo() << "FollowAlong: outside click, hiding";
        m_window->hide();
        if (m_monitor)
            m_monitor->resetSelectionState();
        return;
    }

    // Without a global cursor position, correlate the active-notify press
    // with a local press on the toolbar window: the local event may be
    // delivered slightly before or after the global notification.
    const qint64 pressedAt = QDateTime::currentMSecsSinceEpoch();
    const quint64 serial = m_window->localPressSerial();
    const quint64 generation = m_window->generation();
    QTimer::singleShot(80, this, [this, pressedAt, serial, generation]() {
        if (!m_window || !m_window->visible())
            return;
        if (m_window->generation() != generation)
            return; // the window was rebuilt for the icon -> pill transition
        if (m_window->property("menuOpen").toBool())
            return;
        // The local press may arrive before or after the global notification.
        if (m_window->localPressSerial() != serial
            || m_window->lastLocalPressMs() >= pressedAt - 200) {
            return; // the press belongs to the toolbar
        }
        qInfo() << "FollowAlong: outside click, hiding";
        m_window->hide();
        if (m_monitor)
            m_monitor->resetSelectionState();
    });
}

void FollowAlongService::showPending()
{
    if (!m_enabled || m_pendingText.trimmed().isEmpty())
        return;
    qInfo() << "FollowAlong: showing for selection length"
            << m_pendingText.size() << "at" << m_pendingPos;
    m_window->show(m_pendingText, m_pendingPos, m_actions);
}

void FollowAlongService::onSelectionCleared()
{
    // Once the user has opened the toolbar, the selected text is already
    // captured. Some applications clear their PRIMARY selection when they
    // lose focus after the click; that must not close the opened toolbar.
    if (m_window && m_window->visible()
        && m_window->property("expanded").toBool()) {
        return;
    }
    // Defer close for the same reason as show().
    QTimer::singleShot(0, m_window.get(), [this]() {
        if (m_window)
            m_window->hide();
    });
}

void FollowAlongService::onActionTriggered(int type, bool custom,
                                           const QString &text)
{
    if (text.trimmed().isEmpty())
        return;

    qInfo() << "FollowAlong: action" << type << "custom" << custom
            << "text" << text.size() << "chars";
    if (m_monitor)
        m_monitor->resetSelectionState();

    if (type == 0) {
        // UOS AI's 搜索 action opens the system browser instead of sending a
        // model request.
        QString query = text;
        query.replace(QLatin1Char('\n'), QLatin1Char(' '));
        const QUrl url(QStringLiteral("https://www.sou.com/?q=")
                       + QString::fromLatin1(QUrl::toPercentEncoding(query)));
        QDesktopServices::openUrl(url);
        return;
    }
    if (type == 8) {
        // The aish backend in this build intentionally has no knowledge-base
        // ingestion API. Keep the action honest instead of pretending to add
        // a file to a knowledge base.
        if (m_app)
            m_app->showToast(QStringLiteral("当前版本不支持知识库导入"));
        return;
    }

    m_pendingText = text;
    if (m_app && m_app->startFollowAlongAction(type, text, custom)) {
        Q_EMIT showChatRequested();
    }
}
