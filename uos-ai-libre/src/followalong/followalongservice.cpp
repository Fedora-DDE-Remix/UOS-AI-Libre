// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "followalongservice.h"

#include "appcontroller.h"
#include "followalongcontroller.h"
#include "followalongeventbus.h"
#include "followalongeventsource.h"
#include "followalongview.h"

#include <QCursor>
#include <QDateTime>
#include <QDesktopServices>
#include <QDebug>
#include <QQmlEngine>
#include <QSettings>
#include <QTimer>
#include <QUrl>
#include <QUrlQuery>
#include <QVariantMap>

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
    : QObject(parent), m_app(app), m_bus(std::make_unique<FollowAlongEventBus>())
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

    m_surface.reset(FollowAlongSurface::create(engine, m_bus.get()));
    if (!m_surface) {
        qWarning().noquote()
            << "FollowAlong unavailable: no valid overlay backend for this "
               "platform";
        return;
    }

    m_source.reset(FollowAlongEventSource::create(m_bus.get()));
    m_controller = std::make_unique<FollowAlongController>(
        m_bus.get(), m_surface.get());
    m_controller->setActions(m_actions, m_moreActions);
    m_controller->setEnabled(m_enabled);

    connect(m_source.get(), &FollowAlongEventSource::monitorError, this,
            [](const QString &message) {
                qWarning().noquote() << "FollowAlong event source:" << message;
            });
    connect(m_controller.get(),
            &FollowAlongController::selectionResetRequested, this, [this]() {
                if (m_source)
                    m_source->resetSelectionState();
            });
    connect(m_controller.get(), &FollowAlongController::presentationFailed,
            this, [](const QString &reason) {
                qWarning().noquote()
                    << "FollowAlong presentation contract:" << reason;
            });
    connect(m_surface.get(), &FollowAlongSurface::actionTriggered, this,
            &FollowAlongService::onActionTriggered);
    connect(m_surface.get(), &FollowAlongSurface::askRequested, this,
            &FollowAlongService::onAskRequested);
    connect(m_surface.get(), &FollowAlongSurface::settingsRequested, this,
            &FollowAlongService::openSettings);
}

FollowAlongService::~FollowAlongService()
{
    if (m_source)
        m_source->stop();
}

bool FollowAlongService::enabled() const { return m_enabled; }

QVariantList FollowAlongService::actions() const { return m_actions; }

QVariantList FollowAlongService::moreActions() const { return m_moreActions; }

void FollowAlongService::ask(const QString &question)
{
    const QString text = m_controller ? m_controller->selectedText() : QString();
    qInfo() << "FollowAlong: ask" << question << "selection" << text.size();
    if (question.trimmed().isEmpty() || text.trimmed().isEmpty())
        return;
    if (m_controller)
        m_controller->dismiss();
    // Show the main window even when the action cannot start (for example
    // while another answer is generating or before the model is ready).
    // Otherwise the error toast is emitted into a hidden window and the
    // toolbar simply disappears without any visible feedback.
    Q_EMIT showChatRequested();
    if (m_app) {
        const bool started = m_app->startFollowAlongAction(
            -1, text, true,
            question + QStringLiteral("\n\n选中内容：${SELECTION}"));
        qInfo() << "FollowAlong: ask started" << started;
    }
}

void FollowAlongService::onAskRequested(const QString &question,
                                        const QString &text)
{
    qInfo() << "FollowAlong: ask" << question << "selection" << text.size();
    if (question.trimmed().isEmpty() || text.trimmed().isEmpty())
        return;
    if (m_controller)
        m_controller->dismiss();
    // Show the main window even when the action cannot start (for example
    // while another answer is generating or before the model is ready).
    // Otherwise the error toast is emitted into a hidden window and the
    // toolbar simply disappears without any visible feedback.
    Q_EMIT showChatRequested();
    if (m_app) {
        const bool started = m_app->startFollowAlongAction(
            -1, text, true,
            question + QStringLiteral("\n\n选中内容：${SELECTION}"));
        qInfo() << "FollowAlong: ask started" << started;
    }
}

void FollowAlongService::openSettings()
{
    if (m_controller)
        m_controller->dismiss();
    if (m_source)
        m_source->resetSelectionState();
    Q_EMIT settingsRequested();
}

void FollowAlongService::showPreview()
{
    m_previewText = QStringLiteral("这是一段用于验证 AI 随航工具栏的选中文本。");
    m_previewAnchor = QCursor::pos();
    const QString anchor =
        qEnvironmentVariable("UOS_AI_LIBRE_FOLLOWALONG_PREVIEW_ANCHOR");
    if (!anchor.isEmpty()) {
        const QStringList parts = anchor.split(QLatin1Char(','));
        if (parts.size() == 2)
            m_previewAnchor = QPoint(parts.at(0).trimmed().toInt(),
                                     parts.at(1).trimmed().toInt());
    }

    const int preview =
        qEnvironmentVariableIntValue("UOS_AI_LIBRE_FOLLOWALONG_PREVIEW");
    m_previewExpanded = preview == 2 || preview == 4;
    if (!m_controller)
        return;
    m_controller->preview(m_previewText, m_previewAnchor, m_previewExpanded);

    if (preview == 3) {
        QTimer::singleShot(500, this, [this]() {
            if (m_controller)
                m_controller->expand();
        });
    } else if (preview == 4) {
        QTimer::singleShot(700, this, [this]() {
            if (m_surface)
                m_surface->openMoreMenu();
        });
    } else if (preview == 5) {
        QTimer::singleShot(800, this, [this]() {
            if (m_controller)
                m_controller->dismiss();
        });
    } else if (preview == 6) {
        QTimer::singleShot(800, this, [this]() {
            if (m_controller)
                m_controller->dismiss();
        });
        QTimer::singleShot(1600, this, [this]() {
            if (m_controller)
                m_controller->preview(m_previewText, m_previewAnchor, false);
        });
        QTimer::singleShot(2400, this, [this]() {
            if (m_controller)
                m_controller->dismiss();
        });
    }
}

void FollowAlongService::activateToolbar()
{
    if (m_surface)
        m_surface->requestTextInputFocus();
}

void FollowAlongService::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    QSettings config = settings();
    config.setValue(QStringLiteral("enabled"), enabled);
    if (m_controller)
        m_controller->setEnabled(enabled);
    if (!enabled) {
        if (m_sourceStarted && m_source) {
            m_source->stop();
            m_sourceStarted = false;
        }
    } else if (m_started && m_source && !m_sourceStarted) {
        QString error;
        if (m_source->start(&error)) {
            m_sourceStarted = true;
        } else {
            qWarning().noquote() << "FollowAlong unavailable:" << error;
        }
    }
    Q_EMIT enabledChanged();
}

void FollowAlongService::start()
{
    if (m_started)
        return;
    m_started = true;

    if (!m_source || !m_surface || !m_controller) {
        qWarning().noquote() << "FollowAlong unavailable: backend missing";
        return;
    }
    if (!m_enabled) {
        qInfo() << "FollowAlong: disabled by settings";
        return;
    }

    QString error;
    if (!m_source->start(&error)) {
        qWarning().noquote() << "FollowAlong unavailable:" << error;
        return;
    }
    m_sourceStarted = true;

    qInfo().noquote() << "FollowAlong backend:" << m_surface->backendName();
    const FollowAlongCapabilities sourceCaps = m_source->capabilities();
    const FollowAlongCapabilities surfaceCaps = m_surface->capabilities();
    qInfo() << "FollowAlong capabilities:"
            << "globalCursor" << (sourceCaps.globalCursorPosition
                                  && surfaceCaps.globalCursorPosition)
            << "releaseGating" << sourceCaps.releaseGating
            << "overlayRole" << surfaceCaps.overlayRole
            << "focusFreeShow" << surfaceCaps.focusFreeShow
            << "globalHitTest" << (sourceCaps.globalHitTest
                                   && surfaceCaps.globalHitTest);
}

void FollowAlongService::onActionTriggered(int type, bool custom,
                                           const QString &text)
{
    if (text.trimmed().isEmpty())
        return;

    qInfo() << "FollowAlong: action" << type << "custom" << custom
            << "text" << text.size() << "chars";
    if (m_controller)
        m_controller->dismiss();

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
        // a file to a knowledge base, and show the window so the toast is
        // actually visible.
        Q_EMIT showChatRequested();
        if (m_app)
            m_app->showToast(QStringLiteral("当前版本不支持知识库导入"));
        return;
    }

    // Show the chat window before starting the request. If the request is
    // rejected (busy, model unavailable, bridge not ready), the toast emitted
    // by AppController must have a visible window to land in.
    Q_EMIT showChatRequested();
    if (m_app) {
        const bool started =
            m_app->startFollowAlongAction(type, text, custom);
        qInfo() << "FollowAlong: action started" << started;
    }
}
