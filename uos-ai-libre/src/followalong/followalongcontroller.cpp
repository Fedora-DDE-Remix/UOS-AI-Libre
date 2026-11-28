// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "followalongcontroller.h"

#include "followalongeventbus.h"
#include "followalongview.h"

#include <QDebug>
#include <QTimer>

namespace {
constexpr int kPresentationTimeoutMs = 2000;
}

FollowAlongController::FollowAlongController(FollowAlongEventBus *bus,
                                             FollowAlongSurface *surface,
                                             QObject *parent)
    : QObject(parent), m_bus(bus), m_surface(surface)
    , m_timeout(new QTimer(this))
{
    m_timeout->setSingleShot(true);
    m_timeout->setInterval(kPresentationTimeoutMs);
    connect(m_timeout, &QTimer::timeout, this, [this]() {
        if (m_state == State::PresentingIcon
            || m_state == State::PresentingExpanded) {
            failPresentation(QStringLiteral("presentation timed out"));
        }
    });

    if (m_bus) {
        connect(m_bus, &FollowAlongEventBus::selectionEvent, this,
                &FollowAlongController::onSelectionEvent);
        connect(m_bus, &FollowAlongEventBus::presentationEvent, this,
                &FollowAlongController::onPresentationEvent);
    }
    if (m_surface) {
        connect(m_surface, &FollowAlongSurface::expandRequested, this,
                &FollowAlongController::expand);
        connect(m_surface, &FollowAlongSurface::dismissRequested, this,
                &FollowAlongController::dismiss);
    }
}

FollowAlongController::~FollowAlongController() = default;

bool FollowAlongController::enabled() const { return m_enabled; }

void FollowAlongController::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    if (!enabled)
        dismiss();
}

void FollowAlongController::setActions(const QVariantList &actions,
                                       const QVariantList &moreActions)
{
    m_actions = actions;
    m_moreActions = moreActions;
}

bool FollowAlongController::visible() const
{
    // "Visible" means the presentation contract has been confirmed, not just
    // that a command was sent to the backend.
    return m_state == State::IconShown || m_state == State::Expanded;
}

bool FollowAlongController::expanded() const
{
    return m_state == State::Expanded;
}

QString FollowAlongController::selectedText() const { return m_capturedText; }

void FollowAlongController::setState(State state)
{
    if (m_state == state)
        return;
    const bool wasVisible = visible();
    const bool wasExpanded = expanded();
    m_state = state;
    if (wasVisible != visible())
        Q_EMIT visibleChanged();
    if (wasExpanded != expanded())
        Q_EMIT expandedChanged();
}

void FollowAlongController::onSelectionEvent(const SelectionEvent &event)
{
    if (!m_enabled) {
        if (event.phase == SelectionPhase::Cleared && visible())
            dismiss();
        return;
    }

    switch (event.phase) {
    case SelectionPhase::GestureBegan:
        // The surface owns outside-click hit testing. While it is visible a
        // press either belongs to the toolbar (surface suppresses it) or it
        // arrives as dismissRequested. The controller must not arm a new
        // selection in that case.
        if (m_state == State::Idle)
            setState(State::Armed);
        break;

    case SelectionPhase::TextPublished:
        if (event.text.trimmed().isEmpty())
            break;
        m_capturedText = event.text;
        m_anchor = event.globalAnchor.value_or(QPoint());
        m_hasAnchor = event.globalAnchor.has_value();
        Q_EMIT selectedTextChanged();
        if (m_state == State::Idle || m_state == State::Armed)
            setState(State::Captured);
        break;

    case SelectionPhase::GestureEnded:
        if (m_capturedText.trimmed().isEmpty()
            && !event.text.trimmed().isEmpty()) {
            m_capturedText = event.text;
            m_anchor = event.globalAnchor.value_or(QPoint());
            m_hasAnchor = event.globalAnchor.has_value();
            Q_EMIT selectedTextChanged();
        }
        if (m_capturedText.trimmed().isEmpty()) {
            if (m_state == State::Armed)
                setState(State::Idle);
            break;
        }
        if (m_state == State::Captured || m_state == State::Armed)
            presentIcon();
        break;

    case SelectionPhase::Cleared:
        if (event.causedByOwnSurface)
            break;
        // Once the expanded toolbar owns the captured text, a source app
        // clearing PRIMARY after focus changes must not close it.
        if (m_state == State::Expanded
            || m_state == State::PresentingExpanded) {
            break;
        }
        if (m_state == State::Idle || m_state == State::Armed
            || m_state == State::Captured) {
            clearSelection();
            setState(State::Idle);
            break;
        }
        // The icon is still shown. A real selection clear (not caused by our
        // own surface) means the source is gone; hide it.
        dismiss();
        break;
    }
}

void FollowAlongController::presentIcon()
{
    if (!m_surface || m_capturedText.trimmed().isEmpty())
        return;

    const quint64 requestId = ++m_nextRequestId;
    m_pendingRequestId = requestId;
    m_accepted = false;
    m_roleConfirmed = false;
    m_mapped = false;
    m_geometryReceived = false;
    m_focusOk = false;
    m_lastError.clear();
    setState(State::PresentingIcon);

    FollowAlongContent content;
    content.selectedText = m_capturedText;
    content.actions = m_actions;
    content.moreActions = m_moreActions;
    content.expanded = false;

    const FollowAlongPresentation spec = basePresentation(false);
    qInfo() << "FollowAlong: presenting icon, text" << m_capturedText.size()
            << "anchor" << m_anchor << "exact" << m_hasAnchor;
    const PresentationCommandResult result =
        m_surface->present(requestId, spec, content);
    if (!result.accepted) {
        failPresentation(result.error.isEmpty()
                             ? QStringLiteral("present() rejected")
                             : result.error);
        return;
    }
    m_timeout->start();
}

void FollowAlongController::presentExpanded()
{
    if (!m_surface || m_capturedText.trimmed().isEmpty())
        return;

    const quint64 requestId = ++m_nextRequestId;
    m_pendingRequestId = requestId;
    m_accepted = false;
    m_mapped = false;
    m_geometryReceived = false;
    m_focusOk = true; // expanded may accept focus on explicit input
    m_lastError.clear();
    setState(State::PresentingExpanded);

    FollowAlongContent content;
    content.selectedText = m_capturedText;
    content.actions = m_actions;
    content.moreActions = m_moreActions;
    content.expanded = true;

    FollowAlongPresentation spec = basePresentation(true);
    spec.focusPolicy = FollowAlongPresentation::FocusPolicy::OnExplicitRequest;
    spec.size = QSize(425, 40);
    qInfo() << "FollowAlong: presenting expanded at" << m_anchor;
    const PresentationCommandResult result =
        m_surface->update(requestId, spec, content);
    if (!result.accepted) {
        failPresentation(result.error.isEmpty()
                             ? QStringLiteral("update() rejected")
                             : result.error);
        return;
    }
    m_timeout->start();
}

FollowAlongPresentation FollowAlongController::basePresentation(
    bool expanded) const
{
    FollowAlongPresentation spec;
    spec.layer = FollowAlongPresentation::Layer::Overlay;
    spec.alwaysOnTop = true;
    spec.showInTaskSwitcher = false;
    spec.showInDockPreview = false;
    spec.showInMultitaskView = false;
    spec.focusPolicy = FollowAlongPresentation::FocusPolicy::Never;
    if (m_hasAnchor) {
        spec.anchorMode = FollowAlongPresentation::AnchorMode::GlobalPoint;
        spec.anchor = m_anchor;
    } else {
        spec.anchorMode = FollowAlongPresentation::AnchorMode::None;
    }
    spec.size = expanded ? QSize(425, 40) : QSize(24, 24);
    spec.clampToOutput = true;
    return spec;
}

void FollowAlongController::expand()
{
    if (m_state == State::IconShown)
        presentExpanded();
}

void FollowAlongController::dismiss()
{
    if (m_surface && (visible() || m_surface->visible()))
        m_surface->dismiss();
    m_timeout->stop();
    m_pendingRequestId = 0;
    const bool changed = m_state != State::Idle;
    clearSelection();
    setState(State::Idle);
    if (changed)
        Q_EMIT selectionResetRequested();
}

void FollowAlongController::onPresentationEvent(
    const PresentationEvent &event)
{
    if (event.requestId != m_pendingRequestId) {
        // Stale request result: never let an old surface move the new state.
        return;
    }

    switch (event.type) {
    case PresentationEventType::Accepted:
        m_accepted = true;
        break;
    case PresentationEventType::Rejected:
    case PresentationEventType::Failed:
        m_lastError = event.error;
        failPresentation(event.error.isEmpty()
                             ? QStringLiteral("presentation failed")
                             : event.error);
        return;
    case PresentationEventType::RoleConfirmed:
        m_roleConfirmed = event.state.overlayEffective;
        break;
    case PresentationEventType::Mapped:
        m_mapped = true;
        break;
    case PresentationEventType::FocusChanged:
        m_focusOk = !event.state.focused;
        break;
    case PresentationEventType::GeometryChanged:
        m_geometryReceived = event.state.geometry.has_value();
        break;
    case PresentationEventType::Dismissed:
        if (m_state != State::Idle) {
            m_timeout->stop();
            m_pendingRequestId = 0;
            clearSelection();
            setState(State::Idle);
        }
        return;
    }
    maybeFinishPresentation();
}

void FollowAlongController::maybeFinishPresentation()
{
    if (m_state == State::PresentingIcon) {
        if (!m_accepted || !m_roleConfirmed || !m_mapped || !m_focusOk)
            return;
        m_timeout->stop();
        qInfo() << "FollowAlong: icon presentation confirmed";
        setState(State::IconShown);
        Q_EMIT presentedIcon();
        return;
    }

    if (m_state == State::PresentingExpanded) {
        if (!m_accepted || !m_mapped || !m_geometryReceived)
            return;
        m_timeout->stop();
        qInfo() << "FollowAlong: expanded presentation confirmed";
        setState(State::Expanded);
        return;
    }
}

void FollowAlongController::failPresentation(const QString &reason)
{
    m_lastError = reason;
    qWarning().noquote() << "FollowAlong presentation failed:" << reason
                         << "backend"
                         << (m_surface ? m_surface->backendName()
                                       : QStringLiteral("none"));
    m_timeout->stop();
    if (m_surface)
        m_surface->dismiss();
    m_pendingRequestId = 0;
    clearSelection();
    setState(State::Idle);
    Q_EMIT selectionResetRequested();
    Q_EMIT presentationFailed(reason);
}

void FollowAlongController::clearSelection()
{
    if (m_capturedText.isEmpty())
        return;
    m_capturedText.clear();
    m_anchor = QPoint();
    m_hasAnchor = false;
    Q_EMIT selectedTextChanged();
}

void FollowAlongController::preview(const QString &text, const QPoint &anchor,
                                    bool expanded)
{
    // Test/preview hook: feed the same semantic path but bypass the selection
    // source. The presentation is still validated by PresentationEvents.
    m_capturedText = text;
    m_anchor = anchor;
    m_hasAnchor = true;
    Q_EMIT selectedTextChanged();
    if (expanded)
        presentExpanded();
    else
        presentIcon();
}
