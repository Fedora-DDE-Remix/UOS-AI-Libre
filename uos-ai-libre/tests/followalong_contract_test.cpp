// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "followalongcontroller.h"
#include "followalongeventbus.h"
#include "followalongview.h"

#include <QCoreApplication>
#include <QDebug>
#include <QEventLoop>
#include <QRect>
#include <QTimer>

namespace {

int failures = 0;

void check(bool condition, const char *message)
{
    if (condition)
        return;
    ++failures;
    qCritical() << "FAIL:" << message;
}

void wait(int milliseconds)
{
    QEventLoop loop;
    QTimer::singleShot(milliseconds, &loop, &QEventLoop::quit);
    loop.exec();
}

class FakeSurface final : public FollowAlongSurface
{
public:
    explicit FakeSurface(FollowAlongEventBus *bus)
        : FollowAlongSurface(nullptr, bus)
    {
        m_backendName = QStringLiteral("fake-contract-surface");
    }

    FollowAlongCapabilities capabilities() const override
    {
        FollowAlongCapabilities caps;
        caps.overlayRole = true;
        caps.focusFreeShow = true;
        caps.globalCursorPosition = true;
        caps.globalHitTest = true;
        caps.backendName = m_backendName;
        return caps;
    }

    PresentationCommandResult present(
        quint64 requestId, const FollowAlongPresentation &spec,
        const FollowAlongContent &content) override
    {
        lastRequestId = requestId;
        lastSpec = spec;
        lastContent = content;
        presentCount++;
        return {true, requestId, QString()};
    }

    PresentationCommandResult update(
        quint64 requestId, const FollowAlongPresentation &spec,
        const FollowAlongContent &content) override
    {
        lastRequestId = requestId;
        lastSpec = spec;
        lastContent = content;
        updateCount++;
        return {true, requestId, QString()};
    }

    void dismiss() override
    {
        ++dismissCount;
        FollowAlongPresentationState state;
        state.accepted = true;
        state.overlayEffective = true;
        state.geometry = QRect(100, 100, 24, 24);
        postPresentation(PresentationEventType::Dismissed, lastRequestId, state);
    }

    void requestTextInputFocus() override { ++focusRequests; }
    QRect geometry() const override { return QRect(100, 100, 24, 24); }
    bool isFocused() const override { return false; }

    void succeed(quint64 requestId, bool overlay = true,
                 bool focused = false, bool geometry = true)
    {
        FollowAlongPresentationState state;
        state.accepted = true;
        state.overlayEffective = overlay;
        state.focused = focused;
        state.geometry = QRect(100, 100, 24, 24);
        postPresentation(PresentationEventType::Accepted, requestId, state);
        postPresentation(PresentationEventType::RoleConfirmed, requestId,
                         state);
        postPresentation(PresentationEventType::Mapped, requestId, state);
        postPresentation(PresentationEventType::FocusChanged, requestId,
                         state);
        if (geometry)
            postPresentation(PresentationEventType::GeometryChanged, requestId,
                             state);
    }

    void fail(quint64 requestId, const QString &error)
    {
        FollowAlongPresentationState state;
        state.accepted = true;
        state.geometry = QRect(100, 100, 24, 24);
        postPresentation(PresentationEventType::Failed, requestId, state,
                         error);
    }

    quint64 lastRequestId = 0;
    FollowAlongPresentation lastSpec;
    FollowAlongContent lastContent;
    int presentCount = 0;
    int updateCount = 0;
    int dismissCount = 0;
    int focusRequests = 0;
};

void testIconAndExpanded()
{
    FollowAlongEventBus bus;
    FakeSurface surface(&bus);
    FollowAlongController controller(&bus, &surface);

    int iconShown = 0;
    int failed = 0;
    QObject::connect(&controller, &FollowAlongController::presentedIcon,
                     [&iconShown]() { ++iconShown; });
    QObject::connect(&controller,
                     &FollowAlongController::presentationFailed,
                     [&failed](const QString &) { ++failed; });

    controller.preview(QStringLiteral("selected text"), QPoint(100, 100),
                       false);
    wait(30);
    check(surface.presentCount == 1, "icon present called once");
    check(!controller.visible(), "confirmation is required before visible");
    check(!iconShown, "icon not confirmed before events");

    surface.succeed(surface.lastRequestId);
    wait(60);
    check(iconShown == 1, "icon confirmed after role/map/focus events");
    check(failed == 0, "no failure for successful icon");
    check(!controller.expanded(), "icon state is collapsed");

    controller.expand();
    wait(30);
    check(surface.updateCount == 1, "expand calls update once");
    check(surface.lastContent.expanded, "update content is expanded");
    check(surface.lastSpec.focusPolicy
              == FollowAlongPresentation::FocusPolicy::OnExplicitRequest,
          "expanded presentation requests focus on demand");
    check(!controller.expanded(), "expanded waits for presentation events");

    surface.succeed(surface.lastRequestId);
    wait(60);
    check(controller.expanded(), "expanded confirmed after map/geometry");
    check(failed == 0, "no failure for successful expand");
}

void testFailureDoesNotShowIcon()
{
    FollowAlongEventBus bus;
    FakeSurface surface(&bus);
    FollowAlongController controller(&bus, &surface);

    int iconShown = 0;
    QString failure;
    QObject::connect(&controller, &FollowAlongController::presentedIcon,
                     [&iconShown]() { ++iconShown; });
    QObject::connect(&controller,
                     &FollowAlongController::presentationFailed,
                     [&failure](const QString &reason) { failure = reason; });

    controller.preview(QStringLiteral("text"), QPoint(20, 20), false);
    wait(30);
    surface.fail(surface.lastRequestId,
                 QStringLiteral("overlay role not effective"));
    wait(60);
    check(iconShown == 0, "failed presentation never becomes IconShown");
    check(!failure.isEmpty(), "failure reason surfaced to logic layer");
    check(!controller.visible(), "failed presentation is dismissed");
}

void testStaleRequestIgnored()
{
    FollowAlongEventBus bus;
    FakeSurface surface(&bus);
    FollowAlongController controller(&bus, &surface);

    int iconShown = 0;
    int failed = 0;
    QObject::connect(&controller, &FollowAlongController::presentedIcon,
                     [&iconShown]() { ++iconShown; });
    QObject::connect(&controller,
                     &FollowAlongController::presentationFailed,
                     [&failed](const QString &) { ++failed; });

    controller.preview(QStringLiteral("text"), QPoint(30, 30), false);
    wait(20);
    const quint64 first = surface.lastRequestId;
    surface.succeed(first);
    wait(60);
    check(iconShown == 1, "first icon confirmed");

    // An old request result must not destroy the current icon.
    surface.fail(first - 1, QStringLiteral("stale failure"));
    wait(60);
    check(failed == 0, "stale failure ignored");
    check(controller.visible(), "stale result does not hide current surface");

    controller.expand();
    wait(30);
    check(surface.updateCount == 1, "current request can still expand");
    check(surface.lastRequestId > first, "request ids increase monotonically");
    surface.succeed(surface.lastRequestId);
    wait(60);
    check(controller.expanded(), "second request succeeds");
}

void testClearedHandling()
{
    FollowAlongEventBus bus;
    FakeSurface surface(&bus);
    FollowAlongController controller(&bus, &surface);

    controller.preview(QStringLiteral("text"), QPoint(40, 40), false);
    wait(20);
    surface.succeed(surface.lastRequestId);
    wait(60);
    check(controller.visible(), "icon visible before clear tests");

    SelectionEvent ownClear;
    ownClear.phase = SelectionPhase::Cleared;
    ownClear.causedByOwnSurface = true;
    bus.postSelectionEvent(ownClear);
    wait(30);
    check(controller.visible(), "own-surface clear ignored");

    SelectionEvent externalClear;
    externalClear.phase = SelectionPhase::Cleared;
    bus.postSelectionEvent(externalClear);
    wait(30);
    check(!controller.visible(), "external clear dismisses icon");
}

void testExpandedClearGrace()
{
    FollowAlongEventBus bus;
    FakeSurface surface(&bus);
    FollowAlongController controller(&bus, &surface);

    controller.preview(QStringLiteral("text"), QPoint(60, 60), false);
    wait(20);
    surface.succeed(surface.lastRequestId);
    wait(60);
    controller.expand();
    wait(20);
    surface.succeed(surface.lastRequestId);
    wait(60);
    check(controller.expanded(), "expanded before clear");

    SelectionEvent clear;
    clear.phase = SelectionPhase::Cleared;
    bus.postSelectionEvent(clear);
    wait(30);
    check(controller.expanded(), "expanded survives PRIMARY clear");
    check(controller.visible(), "expanded remains visible after clear");

    controller.dismiss();
    wait(30);
    check(!controller.visible(), "explicit dismiss still hides expanded");
}

void testTimeout()
{
    FollowAlongEventBus bus;
    FakeSurface surface(&bus);
    FollowAlongController controller(&bus, &surface);

    QString failure;
    QObject::connect(&controller,
                     &FollowAlongController::presentationFailed,
                     [&failure](const QString &reason) { failure = reason; });

    controller.preview(QStringLiteral("text"), QPoint(50, 50), false);
    wait(2150);
    check(!failure.isEmpty(), "missing presentation events time out");
    check(!controller.visible(), "timed-out presentation is dismissed");
    check(surface.dismissCount >= 1, "timeout dismisses the backend");
}

} // namespace

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);

    testIconAndExpanded();
    testFailureDoesNotShowIcon();
    testStaleRequestIgnored();
    testClearedHandling();
    testExpandedClearGrace();
    testTimeout();

    if (failures == 0)
        qInfo() << "followalong-contract-test: PASS";
    return failures == 0 ? 0 : 1;
}
