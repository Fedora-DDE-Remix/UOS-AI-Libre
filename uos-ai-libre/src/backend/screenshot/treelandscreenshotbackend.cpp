#include "treelandscreenshotbackend.h"

#include <QDebug>
#include <QEventLoop>
#include <QGuiApplication>
#include <QTimer>

#ifdef UOS_AI_LIBRE_HAVE_TREELAND_CAPTURE
#include <qwayland-treeland-capture-unstable-v1.h>

#include <private/qguiapplication_p.h>
#include <private/qwaylandclientextension_p.h>
#include <private/qwaylanddisplay_p.h>
#include <private/qwaylandintegration_p.h>
#include <private/qwaylandshmbackingstore_p.h>

#include <QTransform>

namespace {

QtWaylandClient::QWaylandIntegration *waylandIntegration() {
    return dynamic_cast<QtWaylandClient::QWaylandIntegration *>(
        QGuiApplicationPrivate::platformIntegration());
}

QtWaylandClient::QWaylandDisplay *waylandDisplay() {
    auto *integration = waylandIntegration();
    return integration ? integration->display() : nullptr;
}

class CaptureFrame final : public QObject,
                           public QtWayland::treeland_capture_frame_v1 {
public:
    CaptureFrame(::treeland_capture_frame_v1 *object, QEventLoop *loop,
                 QImage *image, QString *error, QObject *parent = nullptr)
        : QObject(parent), QtWayland::treeland_capture_frame_v1(object),
          m_loop(loop), m_image(image), m_error(error) {}

    ~CaptureFrame() override {
        delete m_buffer;
        delete m_pending;
        destroy();
    }

protected:
    void treeland_capture_frame_v1_buffer(uint32_t format, uint32_t width,
                                          uint32_t height,
                                          uint32_t stride) override {
        if (stride != width * 4 || m_pending)
            return;
        auto *display = waylandDisplay();
        if (!display) {
            fail(QStringLiteral("无法访问 Wayland 显示"));
            return;
        }
        m_pending = new QtWaylandClient::QWaylandShmBuffer(
            display, QSize(width, height),
            QtWaylandClient::QWaylandShm::formatFrom(
                static_cast<wl_shm_format>(format)));
        copy(m_pending->buffer());
    }

    void treeland_capture_frame_v1_flags(uint32_t flags) override {
        m_flags = flags;
    }

    void treeland_capture_frame_v1_ready() override {
        if (!m_pending) {
            fail(QStringLiteral("截图帧为空"));
            return;
        }
        QImage image = m_pending->image()->copy();
        constexpr uint32_t YInverted = 0x1;
        if (m_flags & YInverted)
            image = image.transformed(QTransform().scale(1, -1));
        *m_image = image;
        delete m_pending;
        m_pending = nullptr;
        m_loop->quit();
    }

    void treeland_capture_frame_v1_failed() override {
        fail(QStringLiteral("Treeland 截图帧复制失败"));
    }

private:
    void fail(const QString &message) {
        if (m_error)
            *m_error = message;
        m_loop->quit();
    }

    QEventLoop *m_loop;
    QImage *m_image;
    QString *m_error;
    QtWaylandClient::QWaylandShmBuffer *m_buffer = nullptr;
    QtWaylandClient::QWaylandShmBuffer *m_pending = nullptr;
    uint32_t m_flags = 0;
};

class CaptureContext final : public QObject,
                             public QtWayland::treeland_capture_context_v1 {
public:
    CaptureContext(::treeland_capture_context_v1 *object, QEventLoop *loop,
                   QImage *image, QString *error, QObject *parent = nullptr)
        : QObject(parent), QtWayland::treeland_capture_context_v1(object),
          m_loop(loop), m_image(image), m_error(error) {}

    ~CaptureContext() override {
        delete m_frame;
        destroy();
    }

    void selectSource() {
        // Tell the compositor to show its own source selector. Output, window
        // and region are all accepted; the user decides.
        select_source(source_type_output | source_type_window | source_type_region,
                      1, 0, nullptr);
    }

protected:
    void treeland_capture_context_v1_source_ready(int32_t region_x,
                                                  int32_t region_y,
                                                  uint32_t region_width,
                                                  uint32_t region_height,
                                                  uint32_t source_type) override {
        Q_UNUSED(region_x)
        Q_UNUSED(region_y)
        Q_UNUSED(region_width)
        Q_UNUSED(region_height)
        Q_UNUSED(source_type)
        m_frame = new CaptureFrame(capture(), m_loop, m_image, m_error, this);
    }

    void treeland_capture_context_v1_source_failed(uint32_t reason) override {
        if (m_error) {
            *m_error = reason == source_failure_user_cancel
                           ? QStringLiteral("已取消截图")
                           : QStringLiteral("无法选择截图源（%1）").arg(reason);
        }
        m_loop->quit();
    }

private:
    QEventLoop *m_loop;
    QImage *m_image;
    QString *m_error;
    CaptureFrame *m_frame = nullptr;
};

class CaptureManager final
    : public QWaylandClientExtensionTemplate<CaptureManager>,
      public QtWayland::treeland_capture_manager_v1 {
public:
    CaptureManager() : QWaylandClientExtensionTemplate<CaptureManager>(1) {}
};

CaptureManager &captureManager() {
    static CaptureManager manager;
    return manager;
}

} // namespace

QString TreelandScreenshotBackend::name() const {
    return QStringLiteral("treeland");
}

bool TreelandScreenshotBackend::available() const {
    return captureManager().isActive();
}

bool TreelandScreenshotBackend::capture(QImage *image, QString *error) {
    if (!image)
        return false;
    image->fill(Qt::transparent);
    if (error)
        error->clear();

    auto &manager = captureManager();
    if (!manager.isActive()) {
        if (error)
            *error = QStringLiteral("Treeland 截图协议不可用");
        return false;
    }

    QEventLoop loop;
    QTimer timeout;
    timeout.setSingleShot(true);
    QObject::connect(&timeout, &QTimer::timeout, &loop, &QEventLoop::quit);

    CaptureContext context(manager.get_context(), &loop, image, error);
    context.selectSource();
    timeout.start(60000);
    loop.exec();

    if (image->isNull()) {
        if (error && error->isEmpty())
            *error = QStringLiteral("截图超时或已取消");
        return false;
    }
    return true;
}

#else

QString TreelandScreenshotBackend::name() const {
    return QStringLiteral("treeland");
}

bool TreelandScreenshotBackend::available() const {
    return false;
}

bool TreelandScreenshotBackend::capture(QImage *image, QString *error) {
    Q_UNUSED(image)
    if (error)
        *error = QStringLiteral("当前构建未启用 Treeland 截图协议");
    return false;
}

#endif
