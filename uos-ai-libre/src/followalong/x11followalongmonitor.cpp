// SPDX-FileCopyrightText: 2026 UOS AI Libre contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "x11followalongmonitor.h"

#include <QClipboard>
#include <QCursor>
#include <QGuiApplication>
#include <QThread>
#include <QTimer>

#ifdef UOS_AI_LIBRE_HAVE_X11_RECORD
#include <X11/Xlib.h>
#include <X11/Xlibint.h>
#include <X11/extensions/record.h>
#endif

#ifdef UOS_AI_LIBRE_HAVE_X11_RECORD
class X11RecordWorker final : public QThread
{
    Q_OBJECT

public:
    explicit X11RecordWorker(QObject *parent = nullptr)
        : QThread(parent)
    {
    }

    ~X11RecordWorker() override
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
    void pressed(const QPoint &pos);
    void released(const QPoint &pos);

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
        XRecordEnableContext(display, context, &X11RecordWorker::callback,
                             reinterpret_cast<XPointer>(this));
        XRecordFreeContext(display, context);
        XCloseDisplay(display);
    }

private:
    static void callback(XPointer ptr, XRecordInterceptData *data)
    {
        auto *self = reinterpret_cast<X11RecordWorker *>(ptr);
        if (data->category == XRecordFromServer) {
            auto *event = reinterpret_cast<xEvent *>(data->data);
            if (event->u.u.type == ButtonPress) {
                Q_EMIT self->pressed(
                    QPoint(event->u.keyButtonPointer.rootX,
                           event->u.keyButtonPointer.rootY));
            } else if (event->u.u.type == ButtonRelease) {
                Q_EMIT self->released(
                    QPoint(event->u.keyButtonPointer.rootX,
                           event->u.keyButtonPointer.rootY));
            }
        }
        XRecordFreeData(data);
    }
};
#endif // UOS_AI_LIBRE_HAVE_X11_RECORD

X11FollowAlongMonitor::X11FollowAlongMonitor(QObject *parent)
    : FollowAlongMonitor(parent)
    , m_debounce(new QTimer(this))
{
    m_debounce->setSingleShot(true);
    m_debounce->setInterval(90);
    connect(m_debounce, &QTimer::timeout, this,
            &X11FollowAlongMonitor::readSelection);
}

X11FollowAlongMonitor::~X11FollowAlongMonitor() { stop(); }

bool X11FollowAlongMonitor::start(QString *error)
{
    m_clipboard = QGuiApplication::clipboard();
    if (!m_clipboard) {
        if (error)
            *error = QStringLiteral("无法访问 X11 PRIMARY 选区");
        return false;
    }

    connect(m_clipboard, &QClipboard::selectionChanged, this,
            &X11FollowAlongMonitor::onSelectionChanged,
            Qt::UniqueConnection);

#ifdef UOS_AI_LIBRE_HAVE_X11_RECORD
    m_record = new X11RecordWorker(this);
    connect(m_record, &X11RecordWorker::available, this, [this]() {
        m_recordAvailable = true;
    });
    connect(m_record, &X11RecordWorker::unavailable, this,
            &X11FollowAlongMonitor::useSelectionFallback);
    connect(m_record, &X11RecordWorker::pressed, this,
            &X11FollowAlongMonitor::onMousePressed);
    connect(m_record, &X11RecordWorker::released, this,
            &X11FollowAlongMonitor::onMouseReleased);
    m_record->start();
    qInfo() << "FollowAlong: XRecord monitor starting";
#else
    useSelectionFallback();
    qInfo() << "FollowAlong: X11 selection fallback";
#endif
    if (error)
        error->clear();
    return true;
}

void X11FollowAlongMonitor::stop()
{
    if (m_clipboard)
        disconnect(m_clipboard, &QClipboard::selectionChanged, this,
                   &X11FollowAlongMonitor::onSelectionChanged);
    m_debounce->stop();
    if (m_record) {
        m_record->requestInterruption();
        m_record->wait(100);
        m_record = nullptr;
    }
}

void X11FollowAlongMonitor::useSelectionFallback()
{
    m_recordAvailable = false;
    m_released = true;
}

void X11FollowAlongMonitor::onMousePressed(const QPoint &pos)
{
    m_pressed = true;
    m_released = false;
    m_moved = false;
    m_pressPos = pos;
    m_pendingText.clear();
}

void X11FollowAlongMonitor::onMouseReleased(const QPoint &pos)
{
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
        return;
    }
    qInfo() << "FollowAlong: X11 release at" << pos;
    m_debounce->start();
}

void X11FollowAlongMonitor::resetSelectionState()
{
    m_lastText.clear();
    m_pendingText.clear();
}

void X11FollowAlongMonitor::onSelectionChanged()
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

void X11FollowAlongMonitor::readSelection()
{
    if (!m_released)
        return;
    if (m_recordAvailable && !m_moved)
        return;

    QString text = m_pendingText;
    if (!m_clipboard) {
        Q_EMIT selectionCleared();
        return;
    }
    text = m_clipboard->text(QClipboard::Selection).trimmed();
    if (text == QStringLiteral("\u0001") || text == QStringLiteral("\ufffc"))
        text.clear();

    if (text.isEmpty()) {
        m_lastText.clear();
        Q_EMIT selectionCleared();
        return;
    }
    if (text == m_lastText)
        return;

    m_lastText = text;
    qInfo() << "FollowAlong: X11 selected" << text.size() << "chars";
    Q_EMIT selectionChanged(text,
                            m_recordAvailable ? m_releasePos : QCursor::pos());
}

#include "x11followalongmonitor.moc"
