// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "x11followalongeventsource.h"

#include "followalongeventbus.h"

#include <QClipboard>
#include <QCursor>
#include <QDateTime>
#include <QGuiApplication>
#include <QThread>
#include <QTimer>

#ifdef UOS_AI_LIBRE_HAVE_X11_RECORD
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/extensions/record.h>
// Xlib's unprefixed macros collide with Qt's enum names in moc output.
#undef None
#undef Bool
#undef Status
#undef CursorShape
#undef KeyPress
#undef KeyRelease
#undef FocusIn
#undef FocusOut
#undef Expose
#endif

#ifdef UOS_AI_LIBRE_HAVE_X11_RECORD
class X11FollowAlongRecordWorker final : public QThread
{
    Q_OBJECT

public:
    explicit X11FollowAlongRecordWorker(QObject *parent = nullptr)
        : QThread(parent)
    {
    }

    ~X11FollowAlongRecordWorker() override
    {
        requestInterruption();
        // XRecordEnableContext blocks in run(). A short wait is enough for
        // normal shutdown; the process exits immediately after.
        wait(100);
        if (isRunning()) {
            terminate();
            wait(100);
        }
    }

Q_SIGNALS:
    void available();
    void unavailable();
    void pressed(const QPoint &pos, bool left);
    void released(const QPoint &pos, bool left);

protected:
    void run() override
    {
        Display *display = XOpenDisplay(nullptr);
        if (!display) {
            Q_EMIT unavailable();
            return;
        }

        XRecordClientSpec clients = XRecordAllClients;
        XRecordRange *range = XRecordAllocRange();
        if (!range) {
            XCloseDisplay(display);
            Q_EMIT unavailable();
            return;
        }
        memset(range, 0, sizeof(XRecordRange));
        range->device_events.first = ButtonPress;
        range->device_events.last = ButtonRelease;

        XRecordContext context =
            XRecordCreateContext(display, 0, &clients, 1, &range, 1);
        XFree(range);
        if (!context) {
            XCloseDisplay(display);
            Q_EMIT unavailable();
            return;
        }

        Q_EMIT available();
        XRecordEnableContext(display, context,
                             &X11FollowAlongRecordWorker::callback,
                             reinterpret_cast<XPointer>(this));
        XRecordFreeContext(display, context);
        XCloseDisplay(display);
    }

private:
    static void callback(XPointer ptr, XRecordInterceptData *data)
    {
        auto *self = reinterpret_cast<X11FollowAlongRecordWorker *>(ptr);
        if (data->category == XRecordFromServer) {
            auto *event = reinterpret_cast<xEvent *>(data->data);
            const bool left = event->u.u.detail == Button1;
            if (event->u.u.type == ButtonPress) {
                Q_EMIT self->pressed(
                    QPoint(event->u.keyButtonPointer.rootX,
                           event->u.keyButtonPointer.rootY),
                    left);
            } else if (event->u.u.type == ButtonRelease) {
                Q_EMIT self->released(
                    QPoint(event->u.keyButtonPointer.rootX,
                           event->u.keyButtonPointer.rootY),
                    left);
            }
        }
        XRecordFreeData(data);
    }
};
#endif // UOS_AI_LIBRE_HAVE_X11_RECORD

X11FollowAlongEventSource::X11FollowAlongEventSource(
    FollowAlongEventBus *bus, QObject *parent)
    : FollowAlongEventSource(bus, parent)
    , m_debounce(new QTimer(this))
{
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(90);
    connect(m_debounce, &QTimer::timeout, this,
            &X11FollowAlongEventSource::readSelection);
}

X11FollowAlongEventSource::~X11FollowAlongEventSource() { stop(); }

bool X11FollowAlongEventSource::start(QString *error)
{
    m_clipboard = QGuiApplication::clipboard();
    if (!m_clipboard) {
        if (error)
            *error = QStringLiteral("无法访问 X11 PRIMARY 选区");
        return false;
    }

    connect(m_clipboard, &QClipboard::selectionChanged, this,
            &X11FollowAlongEventSource::onSelectionChanged,
            Qt::UniqueConnection);

#ifdef UOS_AI_LIBRE_HAVE_X11_RECORD
    m_record = new X11FollowAlongRecordWorker(this);
    connect(m_record, &X11FollowAlongRecordWorker::available, this,
            [this]() { m_recordAvailable = true; });
    connect(m_record, &X11FollowAlongRecordWorker::unavailable, this,
            &X11FollowAlongEventSource::useSelectionFallback);
    connect(m_record, &X11FollowAlongRecordWorker::pressed, this,
            &X11FollowAlongEventSource::onMousePressed);
    connect(m_record, &X11FollowAlongRecordWorker::released, this,
            &X11FollowAlongEventSource::onMouseReleased);
    m_record->start();
    qInfo() << "FollowAlong: XRecord event source starting";
#else
    useSelectionFallback();
    qInfo() << "FollowAlong: X11 selection fallback";
#endif
    if (error)
        error->clear();
    return true;
}

void X11FollowAlongEventSource::stop()
{
    if (m_clipboard)
        disconnect(m_clipboard, &QClipboard::selectionChanged, this,
                   &X11FollowAlongEventSource::onSelectionChanged);
    m_debounce->stop();
    if (m_record) {
        m_record->requestInterruption();
        m_record->wait(100);
        m_record = nullptr;
    }
}

FollowAlongCapabilities X11FollowAlongEventSource::capabilities() const
{
    FollowAlongCapabilities caps;
    caps.globalCursorPosition = true;
    caps.releaseGating = true;
    caps.globalHitTest = true;
    caps.backendName = QStringLiteral("X11 PRIMARY/XRecord");
    return caps;
}

void X11FollowAlongEventSource::useSelectionFallback()
{
    m_recordAvailable = false;
    m_released = true;
}

void X11FollowAlongEventSource::onMousePressed(const QPoint &pos, bool left)
{
    if (!left)
        return;

    m_pressed = true;
    m_released = false;
    m_moved = false;
    m_pressPos = pos;
    m_pendingText.clear();

    SelectionEvent event;
    event.phase = SelectionPhase::GestureBegan;
    event.globalAnchor = pos;
    event.button = Qt::LeftButton;
    event.timestampMs = QDateTime::currentMSecsSinceEpoch();
    post(event);
}

void X11FollowAlongEventSource::onMouseReleased(const QPoint &pos, bool left)
{
    if (!left)
        return;
    m_pressed = false;
    m_releasePos = pos;
    // A plain click over an existing PRIMARY selection (for example on the
    // FollowAlong toolbar or its menu) is not a selection gesture. Requiring
    // real pointer movement keeps the toolbar from re-showing and moving
    // itself when one of its own buttons is clicked.
    m_moved = (pos - m_pressPos).manhattanLength() >= 4;
    m_released = m_moved;
    if (!m_moved) {
        qInfo() << "FollowAlong: X11 click ignored at" << pos;
        SelectionEvent ended;
        ended.phase = SelectionPhase::GestureEnded;
        ended.globalAnchor = pos;
        ended.button = Qt::LeftButton;
        ended.timestampMs = QDateTime::currentMSecsSinceEpoch();
        post(ended);
        return;
    }
    qInfo() << "FollowAlong: X11 release at" << pos;
    m_debounce->start();
}

void X11FollowAlongEventSource::resetSelectionState()
{
    m_lastText.clear();
    m_pendingText.clear();
}

void X11FollowAlongEventSource::onSelectionChanged()
{
    if (!m_clipboard)
        return;

    QString text = m_clipboard->text(QClipboard::Selection).trimmed();
    if (text == QStringLiteral("\u0001") || text == QStringLiteral("\ufffc"))
        text.clear();
    m_pendingText = text;

    // XRecord gives us the real button release. If it is unavailable, fall
    // back to the selection notification so X11 still works.
    if (!m_recordAvailable || m_released)
        m_debounce->start();
}

void X11FollowAlongEventSource::readSelection()
{
    if (!m_released)
        return;
    if (m_recordAvailable && !m_moved)
        return;

    if (!m_clipboard) {
        SelectionEvent cleared;
        cleared.phase = SelectionPhase::Cleared;
        cleared.timestampMs = QDateTime::currentMSecsSinceEpoch();
        post(cleared);
        return;
    }

    QString text = m_clipboard->text(QClipboard::Selection).trimmed();
    if (text == QStringLiteral("\u0001") || text == QStringLiteral("\ufffc"))
        text.clear();

    if (text.isEmpty()) {
        m_lastText.clear();
        SelectionEvent cleared;
        cleared.phase = SelectionPhase::Cleared;
        cleared.timestampMs = QDateTime::currentMSecsSinceEpoch();
        post(cleared);
        return;
    }
    if (text == m_lastText)
        return;

    m_lastText = text;
    qInfo() << "FollowAlong: X11 selected" << text.size() << "chars";
    const QPoint anchor = m_recordAvailable ? m_releasePos : QCursor::pos();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    SelectionEvent published;
    published.phase = SelectionPhase::TextPublished;
    published.text = text;
    published.globalAnchor = anchor;
    published.button = Qt::LeftButton;
    published.fromDrag = true;
    published.timestampMs = now;
    post(published);

    SelectionEvent ended;
    ended.phase = SelectionPhase::GestureEnded;
    ended.text = text;
    ended.globalAnchor = anchor;
    ended.button = Qt::LeftButton;
    ended.fromDrag = true;
    ended.timestampMs = now;
    post(ended);
}

#include "x11followalongeventsource.moc"
