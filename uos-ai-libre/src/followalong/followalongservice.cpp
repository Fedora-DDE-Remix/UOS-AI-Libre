// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "followalongservice.h"

#include "followalongmonitor.h"
#include "followalongwindow.h"

#include "appcontroller.h"

#include <QCursor>
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
    if (m_app && m_app->startFollowAlongAction(
            -1, m_pendingText, true,
            question + QStringLiteral("\n\n选中内容：${SELECTION}"))) {
        Q_EMIT showChatRequested();
    }
}

void FollowAlongService::openSettings()
{
    m_window->hide();
    Q_EMIT settingsRequested();
}

void FollowAlongService::showPreview()
{
    m_pendingText = QStringLiteral("这是一段用于验证 AI 随航工具栏的选中文本。");
    m_pendingPos = QCursor::pos();
    m_window->show(m_pendingText, m_pendingPos, m_actions);
    if (qEnvironmentVariableIntValue("UOS_AI_LIBRE_FOLLOWALONG_PREVIEW") >= 2)
        m_window->setExpanded(true);
}

void FollowAlongService::activateToolbar()
{
    m_window->activateForInput();
}

bool FollowAlongService::cursorInsideToolbar() const
{
    return m_window->cursorInside();
}

bool FollowAlongService::cursorPositionAvailable() const
{
    return m_window->cursorPositionAvailable();
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
