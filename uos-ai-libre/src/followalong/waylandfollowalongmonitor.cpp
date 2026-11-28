// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "waylandfollowalongmonitor.h"

#include <QDateTime>
#include <QGuiApplication>
#include <QLoggingCategory>
#include <QWaylandClientExtension>
#include <qguiapplication_platform.h>

#include <qwayland-wlr-data-control-unstable-v1.h>
#include <qwayland-treeland-active-notify-unstable-v1.h>

#include <wayland-client.h>

#include <cerrno>
#include <cstring>
#include <poll.h>
#include <unistd.h>

Q_LOGGING_CATEGORY(logFollowAlongWayland, "uosai.followalong.wayland")

namespace {

constexpr auto kUtf8Text = "text/plain;charset=utf-8";
constexpr auto kPlainText = "text/plain";

QString readOfferText(QtWayland::zwlr_data_control_offer_v1 *offer,
                      const QString &mime)
{
    if (!offer || mime.isEmpty())
        return {};
    // The generated offer class discovers its formats before the selection
    // event arrives.
    if (!offer->isInitialized())
        return {};

    int fds[2];
    if (pipe(fds) != 0) {
        qCWarning(logFollowAlongWayland) << "pipe failed:" << strerror(errno);
        return {};
    }

    offer->receive(mime, fds[1]);
    close(fds[1]);

    if (auto *app = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>()) {
        if (auto *display = app->display())
            wl_display_flush(display);
    }

    QString result;
    pollfd pfd{fds[0], POLLIN, 0};
    while (true) {
        const int ready = poll(&pfd, 1, 1000);
        if (ready < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (ready == 0) {
            qCWarning(logFollowAlongWayland) << "timed out reading selection";
            break;
        }
        char buffer[4096];
        const ssize_t count = read(fds[0], buffer, sizeof buffer);
        if (count < 0) {
            if (errno == EINTR)
                continue;
            break;
        }
        if (count == 0)
            break;
        result.append(QString::fromUtf8(buffer, static_cast<int>(count)));
    }
    close(fds[0]);
    return result;
}

} // namespace

class WaylandDataOffer final : public QtWayland::zwlr_data_control_offer_v1
{
public:
    explicit WaylandDataOffer(::zwlr_data_control_offer_v1 *id)
        : QtWayland::zwlr_data_control_offer_v1(id)
    {
    }

    ~WaylandDataOffer() override { destroy(); }

    QStringList formats() const { return m_formats; }

protected:
    void zwlr_data_control_offer_v1_offer(const QString &mimeType) override
    {
        m_formats.append(mimeType);
    }

private:
    QStringList m_formats;
};

class WaylandDataControlDevice final
    : public QObject
    , public QtWayland::zwlr_data_control_device_v1
{
    Q_OBJECT

public:
    explicit WaylandDataControlDevice(::zwlr_data_control_device_v1 *id)
        : QtWayland::zwlr_data_control_device_v1(id)
    {
    }

    ~WaylandDataControlDevice() override { destroy(); }

Q_SIGNALS:
    void textSelected(const QString &text);
    void selectionCleared();

protected:
    void zwlr_data_control_device_v1_data_offer(
        ::zwlr_data_control_offer_v1 *id) override
    {
        m_pendingOffer = std::make_unique<WaylandDataOffer>(id);
    }

    void zwlr_data_control_device_v1_selection(
        ::zwlr_data_control_offer_v1 *id) override
    {
        // Some compositors only publish the normal clipboard. FollowAlong is
        // primarily interested in PRIMARY, but reading the clipboard as a
        // fallback makes the feature usable on those compositors too.
        if (!id) {
            if (!m_primaryActive)
                Q_EMIT selectionCleared();
            return;
        }
        m_primaryActive = false;
        readAndEmit();
    }

    void zwlr_data_control_device_v1_primary_selection(
        ::zwlr_data_control_offer_v1 *id) override
    {
        if (!id) {
            m_primaryActive = false;
            Q_EMIT selectionCleared();
            return;
        }
        m_primaryActive = true;
        readAndEmit();
    }

    void zwlr_data_control_device_v1_finished() override {}

private:
    void readAndEmit()
    {
        if (!m_pendingOffer)
            return;

        const QStringList formats = m_pendingOffer->formats();
        const QString utf8 = QString::fromLatin1(kUtf8Text);
        const QString plain = QString::fromLatin1(kPlainText);
        QString mime;
        if (formats.contains(utf8))
            mime = utf8;
        else if (formats.contains(plain))
            mime = plain;
        else
            return;

        QString text = readOfferText(m_pendingOffer.get(), mime).trimmed();
        if (text.isEmpty()) {
            Q_EMIT selectionCleared();
        } else {
            qInfo() << "FollowAlong: Wayland selection" << text.size()
                    << "chars";
            Q_EMIT textSelected(text);
        }
    }

    std::unique_ptr<WaylandDataOffer> m_pendingOffer;
    bool m_primaryActive = false;
};

class WaylandDataControlManager final
    : public QWaylandClientExtensionTemplate<WaylandDataControlManager>
    , public QtWayland::zwlr_data_control_manager_v1
{
    Q_OBJECT

public:
    WaylandDataControlManager()
        : QWaylandClientExtensionTemplate<WaylandDataControlManager>(2)
    {
    }

    void instantiate() { initialize(); }
};

class WaylandActiveNotify final
    : public QObject
    , public QtWayland::treeland_active_notify_v1
{
    Q_OBJECT

public:
    explicit WaylandActiveNotify(
        struct ::treeland_active_notify_v1 *id)
        : QtWayland::treeland_active_notify_v1(id)
    {
    }

    ~WaylandActiveNotify() override { destroy(); }

Q_SIGNALS:
    void mousePressed();
    void mouseReleased();

protected:
    void treeland_active_notify_v1_activity_changed(
        uint32_t reason, uint32_t state) override
    {
        // reason 0 is mouse, state 1 is active and 0 is inactive.
        if (reason != 0)
            return;
        if (state == 1)
            Q_EMIT mousePressed();
        else
            Q_EMIT mouseReleased();
    }
};

class WaylandActiveNotifyManager final
    : public QWaylandClientExtensionTemplate<WaylandActiveNotifyManager>
    , public QtWayland::treeland_active_notify_manager_v1
{
    Q_OBJECT

public:
    WaylandActiveNotifyManager()
        : QWaylandClientExtensionTemplate<WaylandActiveNotifyManager>(1)
    {
    }

    void instantiate() { initialize(); }
};

WaylandFollowAlongMonitor::WaylandFollowAlongMonitor(QObject *parent)
    : FollowAlongMonitor(parent)
{
}

WaylandFollowAlongMonitor::~WaylandFollowAlongMonitor() = default;

bool WaylandFollowAlongMonitor::start(QString *error)
{
    m_manager = std::make_unique<WaylandDataControlManager>();
    connect(m_manager.get(), &QWaylandClientExtension::activeChanged, this,
            [this]() {
                if (!m_manager || !m_manager->isActive())
                    return;
                createDevice();
            });
    m_manager->instantiate();

    m_activeManager = std::make_unique<WaylandActiveNotifyManager>();
    connect(m_activeManager.get(), &QWaylandClientExtension::activeChanged,
            this, [this]() {
                if (!m_activeManager || !m_activeManager->isActive())
                    return;
                createActiveNotifier();
            });
    m_activeManager->instantiate();

    if (!m_manager->isActive()) {
        // Activation can be asynchronous. The activeChanged connection above
        // creates the device once the global is announced.
        qCInfo(logFollowAlongWayland)
            << "waiting for zwlr_data_control_manager_v1";
    } else {
        qInfo() << "FollowAlong: zwlr_data_control_manager_v1 ready";
    }
    if (error)
        error->clear();
    return true;
}

void WaylandFollowAlongMonitor::stop()
{
    m_activeNotify.reset();
    m_activeManager.reset();
    m_device.reset();
    m_manager.reset();
    m_active = false;
    m_hasMouseActivity = false;
    m_mouseActive = false;
    m_mouseGesture = false;
    m_pendingText.clear();
    m_pendingAtMs = 0;
}

void WaylandFollowAlongMonitor::resetSelectionState()
{
    // Keep the gesture state: wlr-data-control and active-notify are
    // asynchronous, and clearing m_mouseGesture here can leave the next
    // release without a pending selection to emit.
    m_pendingText.clear();
    m_pendingAtMs = 0;
}

void WaylandFollowAlongMonitor::createActiveNotifier()
{
    if (!m_activeManager || !m_activeManager->isActive()
        || m_activeNotify)
        return;

    auto *app = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    if (!app || !app->seat())
        return;

    auto *raw = m_activeManager->get_active_notify(app->seat());
    if (!raw) {
        qCWarning(logFollowAlongWayland)
            << "get_active_notify failed; showing on selection";
        return;
    }

    m_activeNotify = std::make_unique<WaylandActiveNotify>(raw);
    m_hasMouseActivity = true;
    qInfo() << "FollowAlong: treeland_active_notify ready";
    connect(m_activeNotify.get(), &WaylandActiveNotify::mousePressed, this,
            [this]() {
                qInfo() << "FollowAlong: Wayland press, pending"
                        << m_pendingText.size() << "active" << m_mouseActive
                        << "gesture" << m_mouseGesture;
                m_mouseActive = true;
                m_mouseGesture = false;
                // wlr-data-control and treeland-active-notify are separate
                // asynchronous channels. A fast drag can deliver the PRIMARY
                // selection just before the press notification; do not throw
                // away that fresh text. Older text is a previous selection and
                // must not re-show the toolbar on a plain click.
                const qint64 now = QDateTime::currentMSecsSinceEpoch();
                if (m_pendingText.isEmpty() || m_pendingAtMs == 0
                    || now - m_pendingAtMs > 400) {
                    m_pendingText.clear();
                    m_pendingAtMs = 0;
                }
                Q_EMIT pointerPressed(QPoint());
            });
    connect(m_activeNotify.get(), &WaylandActiveNotify::mouseReleased, this,
            [this]() {
                qInfo() << "FollowAlong: Wayland release, pending"
                        << m_pendingText.size();
                m_mouseActive = false;
                m_mouseGesture = true;
                if (!m_pendingText.trimmed().isEmpty()) {
                    Q_EMIT selectionChanged(m_pendingText, QPoint());
                    m_pendingText.clear();
                }
                m_pendingAtMs = 0;
            });
}

void WaylandFollowAlongMonitor::createDevice()
{
    if (!m_manager || !m_manager->isActive() || m_device)
        return;

    auto *app = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
    if (!app || !app->seat()) {
        qCWarning(logFollowAlongWayland)
            << "cannot get wl_seat from the Qt Wayland platform";
        return;
    }

    auto *raw = m_manager->get_data_device(app->seat());
    if (!raw) {
        qCWarning(logFollowAlongWayland) << "get_data_device failed";
        return;
    }

    m_device = std::make_unique<WaylandDataControlDevice>(raw);
    connect(m_device.get(), &WaylandDataControlDevice::textSelected, this,
            [this](const QString &text) {
                m_active = true;
                m_pendingText = text;
                m_pendingAtMs = QDateTime::currentMSecsSinceEpoch();
                qInfo() << "FollowAlong: Wayland selection state" << text.size()
                        << "hasActivity" << m_hasMouseActivity << "active"
                        << m_mouseActive << "gesture" << m_mouseGesture;
                // With the private activity notifier, wait for the left
                // button release. Without it, selection notification is the
                // only available trigger.
                if (!m_hasMouseActivity
                    || (!m_mouseActive && m_mouseGesture)) {
                    Q_EMIT selectionChanged(m_pendingText, QPoint());
                    m_pendingText.clear();
                    m_pendingAtMs = 0;
                }
            });
    connect(m_device.get(), &WaylandDataControlDevice::selectionCleared, this,
            [this]() {
                m_active = false;
                m_pendingText.clear();
                m_pendingAtMs = 0;
                Q_EMIT selectionCleared();
            });
}

#include "waylandfollowalongmonitor.moc"
