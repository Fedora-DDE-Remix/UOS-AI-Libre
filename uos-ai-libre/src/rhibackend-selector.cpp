#include "rhibackend-selector.h"

#include <QDebug>
#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QOffscreenSurface>
#include <QOpenGLContext>
#include <QRegularExpression>
#include <QScreen>
#include <QSurfaceFormat>
#include <QStringList>
#include <QVector>
#include <QtGlobal>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#if defined(Q_OS_UNIX) && !defined(Q_OS_MACOS)
#include <dlfcn.h>
#define UOS_AI_LIBRE_HAS_DLOPEN 1
#endif

namespace {

QString readSysfs(const QString &path) {
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return {};
    return QString::fromLatin1(file.readAll()).trimmed();
}

quint32 parseHexId(const QString &text) {
    QString value = text.trimmed();
    if (value.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive))
        value = value.mid(2);
    bool ok = false;
    const quint32 result = value.toUInt(&ok, 16);
    return ok ? result : 0;
}

QString pciIdString(quint32 vendor, quint32 device) {
    return QStringLiteral("%1:%2")
        .arg(vendor, 0, 16)
        .arg(device, 0, 16);
}

// Finds the DRM card currently used for the desktop. On hybrid systems the
// compositor normally scans out from the card with the internal panel, not
// necessarily the discrete Vulkan device with the highest score.
QString desktopGpuId() {
    const QString override = qEnvironmentVariable("UOS_AI_LIBRE_RHI_GPU").trimmed();
    if (!override.isEmpty()) {
        const QStringList parts = override.split(QLatin1Char(':'), Qt::SkipEmptyParts);
        if (parts.size() == 2) {
            const quint32 vendor = parseHexId(parts.at(0));
            const quint32 device = parseHexId(parts.at(1));
            if (vendor != 0 && device != 0)
                return pciIdString(vendor, device);
        }
        qWarning() << "invalid UOS_AI_LIBRE_RHI_GPU value:" << override
                   << "(expected vendor:device in hex, e.g. 1002:1900)";
    }

    // Prefer the card that owns the primary screen's connector. This directly
    // follows what the desktop is presenting on, even when both an internal
    // panel and an external output are connected.
    const QScreen *primaryScreen = QGuiApplication::primaryScreen();
    if (primaryScreen) {
        const QString connectorName = primaryScreen->name().trimmed();
        if (!connectorName.isEmpty()) {
            const QDir drm(QStringLiteral("/sys/class/drm"));
            const QStringList connectors =
                drm.entryList(QStringList{QStringLiteral("card*-*")},
                              QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
            for (const QString &entry : connectors) {
                const int dash = entry.indexOf(QLatin1Char('-'));
                if (dash <= 0)
                    continue;
                if (entry.mid(dash + 1).compare(connectorName, Qt::CaseInsensitive) != 0)
                    continue;
                const QString cardName = entry.left(dash);
                const QString deviceDir = drm.filePath(cardName + QStringLiteral("/device"));
                const quint32 vendor =
                    parseHexId(readSysfs(deviceDir + QStringLiteral("/vendor")));
                const quint32 device =
                    parseHexId(readSysfs(deviceDir + QStringLiteral("/device")));
                if (vendor != 0 && device != 0)
                    return pciIdString(vendor, device);
            }
        }
    }

    struct Card {
        QString name;
        quint32 vendor = 0;
        quint32 device = 0;
        bool bootVga = false;
        bool internal = false;
        int connected = 0;
    };

    const QDir drm(QStringLiteral("/sys/class/drm"));
    static const QRegularExpression cardPattern(QStringLiteral("^card\\d+$"));
    QVector<Card> cards;
    for (const QString &entry :
         drm.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name)) {
        if (!cardPattern.match(entry).hasMatch())
            continue;

        Card card;
        card.name = entry;
        const QString deviceDir = drm.filePath(entry + QStringLiteral("/device"));
        card.vendor = parseHexId(readSysfs(deviceDir + QStringLiteral("/vendor")));
        card.device = parseHexId(readSysfs(deviceDir + QStringLiteral("/device")));
        if (card.vendor == 0 || card.device == 0)
            continue;
        card.bootVga = readSysfs(deviceDir + QStringLiteral("/boot_vga"))
                       == QStringLiteral("1");

        const QStringList connectors = drm.entryList(
            QStringList{entry + QStringLiteral("-*")},
            QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        for (const QString &connector : connectors) {
            if (readSysfs(drm.filePath(connector + QStringLiteral("/status")))
                != QStringLiteral("connected")) {
                continue;
            }
            ++card.connected;
            if (connector.contains(QStringLiteral("eDP"), Qt::CaseInsensitive)
                || connector.contains(QStringLiteral("LVDS"), Qt::CaseInsensitive)) {
                card.internal = true;
            }
        }
        cards.append(card);
    }

    const Card *selected = nullptr;
    for (const Card &card : cards) {
        if (card.internal && card.connected > 0) {
            selected = &card;
            break;
        }
    }
    if (!selected) {
        int connectedCards = 0;
        for (const Card &card : cards) {
            if (card.connected > 0) {
                selected = &card;
                ++connectedCards;
            }
        }
        if (connectedCards != 1)
            selected = nullptr;
    }
    if (!selected) {
        for (const Card &card : cards) {
            if (card.bootVga) {
                selected = &card;
                break;
            }
        }
    }
    if (!selected && !cards.isEmpty())
        selected = &cards.first();
    if (!selected)
        return {};
    return pciIdString(selected->vendor, selected->device);
}

QStringList vulkanSurfaceExtensions() {
    const QString platform = QGuiApplication::platformName();
    QStringList extensions{QStringLiteral("VK_KHR_surface")};
    if (platform == QStringLiteral("wayland"))
        extensions.append(QStringLiteral("VK_KHR_wayland_surface"));
    else if (platform == QStringLiteral("xcb"))
        extensions.append(QStringLiteral("VK_KHR_xcb_surface"));
    else if (platform == QStringLiteral("xlib"))
        extensions.append(QStringLiteral("VK_KHR_xlib_surface"));
    else
        extensions.clear();
    return extensions;
}

#ifdef UOS_AI_LIBRE_HAS_DLOPEN
namespace vk {

using VkResult = int32_t;
using VkInstance = void *;
using VkPhysicalDevice = void *;
using VkFlags = uint32_t;

constexpr VkResult VK_SUCCESS = 0;
constexpr uint32_t VK_STRUCTURE_TYPE_APPLICATION_INFO = 0;
constexpr uint32_t VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO = 1;
constexpr uint32_t VK_QUEUE_GRAPHICS_BIT = 0x00000001;
constexpr uint32_t VK_PHYSICAL_DEVICE_TYPE_CPU = 4;
constexpr uint32_t VK_API_VERSION_1_0 = (1u << 22);

// These are the leading fields of the stable Vulkan 1.0 structs. The loader
// writes the complete structure, so properties are read into a larger buffer.
struct VkApplicationInfo {
    uint32_t sType;
    const void *pNext;
    const char *pApplicationName;
    uint32_t applicationVersion;
    const char *pEngineName;
    uint32_t engineVersion;
    uint32_t apiVersion;
};

struct VkInstanceCreateInfo {
    uint32_t sType;
    const void *pNext;
    VkFlags flags;
    const VkApplicationInfo *pApplicationInfo;
    uint32_t enabledLayerCount;
    const char *const *ppEnabledLayerNames;
    uint32_t enabledExtensionCount;
    const char *const *ppEnabledExtensionNames;
};

struct VkPhysicalDevicePropertiesPrefix {
    uint32_t apiVersion;
    uint32_t driverVersion;
    uint32_t vendorID;
    uint32_t deviceID;
    uint32_t deviceType;
    char deviceName[256];
    uint8_t pipelineCacheUUID[16];
};

struct VkQueueFamilyPropertiesLite {
    uint32_t queueFlags;
    uint32_t queueCount;
    uint32_t timestampValidBits;
    uint32_t minImageTransferGranularity[3];
};

using CreateInstanceFn =
    VkResult (*)(const VkInstanceCreateInfo *, const void *, VkInstance *);
using DestroyInstanceFn = void (*)(VkInstance, const void *);
using EnumeratePhysicalDevicesFn =
    VkResult (*)(VkInstance, uint32_t *, VkPhysicalDevice *);
using GetPhysicalDevicePropertiesFn = void (*)(VkPhysicalDevice, void *);
using GetPhysicalDeviceQueueFamilyPropertiesFn =
    void (*)(VkPhysicalDevice, uint32_t *, VkQueueFamilyPropertiesLite *);

struct Loader {
    void *handle = nullptr;
    ~Loader() {
        if (handle)
            ::dlclose(handle);
    }
};

struct InstanceGuard {
    VkInstance instance = nullptr;
    DestroyInstanceFn destroy = nullptr;
    ~InstanceGuard() {
        if (instance && destroy)
            destroy(instance, nullptr);
    }
};

bool isSoftwareDevice(const QString &name) {
    const QString lower = name.toLower();
    for (const char *needle :
         {"llvmpipe", "lavapipe", "softpipe", "swrast", "swiftshader"}) {
        if (lower.contains(QLatin1String(needle)))
            return true;
    }
    return false;
}

struct ProbeResult {
    bool ok = false;
    bool surfaceOrder = false;
    int firstRealIndex = -1;
    int desktopIndex = -1;
    QString firstRealName;
    QString desktopName;
};

bool hasGraphicsQueue(const VkPhysicalDevice device,
                      GetPhysicalDeviceQueueFamilyPropertiesFn getQueueFamilies) {
    uint32_t familyCount = 0;
    getQueueFamilies(device, &familyCount, nullptr);
    if (familyCount == 0)
        return false;
    std::vector<VkQueueFamilyPropertiesLite> families(familyCount);
    getQueueFamilies(device, &familyCount, families.data());
    return std::any_of(families.cbegin(), families.cend(),
                       [](const VkQueueFamilyPropertiesLite &family) {
                           return family.queueCount > 0
                                  && (family.queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
                       });
}

bool probe(const QString &desktopId, ProbeResult *result) {
    Loader loader;
    loader.handle = ::dlopen("libvulkan.so.1", RTLD_NOW | RTLD_LOCAL);
    if (!loader.handle)
        loader.handle = ::dlopen("libvulkan.so", RTLD_NOW | RTLD_LOCAL);
    if (!loader.handle)
        return false;

    const auto createInstance =
        reinterpret_cast<CreateInstanceFn>(::dlsym(loader.handle, "vkCreateInstance"));
    const auto enumerateDevices = reinterpret_cast<EnumeratePhysicalDevicesFn>(
        ::dlsym(loader.handle, "vkEnumeratePhysicalDevices"));
    const auto getProperties = reinterpret_cast<GetPhysicalDevicePropertiesFn>(
        ::dlsym(loader.handle, "vkGetPhysicalDeviceProperties"));
    const auto getQueueFamilies =
        reinterpret_cast<GetPhysicalDeviceQueueFamilyPropertiesFn>(
            ::dlsym(loader.handle, "vkGetPhysicalDeviceQueueFamilyProperties"));

    InstanceGuard instance;
    instance.destroy = reinterpret_cast<DestroyInstanceFn>(
        ::dlsym(loader.handle, "vkDestroyInstance"));
    if (!createInstance || !enumerateDevices || !getProperties || !getQueueFamilies
        || !instance.destroy) {
        return false;
    }

    const VkApplicationInfo application{
        VK_STRUCTURE_TYPE_APPLICATION_INFO, nullptr, "uos-ai-libre", 0,
        "uos-ai-libre-rhi-probe", 0, VK_API_VERSION_1_0};
    VkInstanceCreateInfo createInfo{
        VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO, nullptr, 0, &application,
        0, nullptr, 0, nullptr};

    const QStringList extensions = vulkanSurfaceExtensions();
    std::vector<QByteArray> extensionNames;
    extensionNames.reserve(size_t(extensions.size()));
    for (const QString &extension : extensions)
        extensionNames.push_back(extension.toLatin1());
    std::vector<const char *> extensionStorage;
    extensionStorage.reserve(extensionNames.size());
    for (const QByteArray &extension : extensionNames)
        extensionStorage.push_back(extension.constData());
    if (!extensionStorage.empty()) {
        createInfo.enabledExtensionCount = uint32_t(extensionStorage.size());
        createInfo.ppEnabledExtensionNames = extensionStorage.data();
    }

    if (createInstance(&createInfo, nullptr, &instance.instance) != VK_SUCCESS
        || !instance.instance) {
        // Surface extensions can be missing on unusual loaders. Fall back to a
        // plain instance, but never rely on its device ordering for selection.
        extensionStorage.clear();
        createInfo.enabledExtensionCount = 0;
        createInfo.ppEnabledExtensionNames = nullptr;
        if (createInstance(&createInfo, nullptr, &instance.instance) != VK_SUCCESS
            || !instance.instance) {
            return false;
        }
    } else {
        result->surfaceOrder = !extensions.isEmpty();
    }

    uint32_t count = 0;
    if (enumerateDevices(instance.instance, &count, nullptr) != VK_SUCCESS
        || count == 0) {
        return false;
    }
    std::vector<VkPhysicalDevice> devices(count);
    if (enumerateDevices(instance.instance, &count, devices.data()) != VK_SUCCESS
        || count == 0) {
        return false;
    }

    for (uint32_t i = 0; i < count; ++i) {
        alignas(8) unsigned char properties[1024]{};
        getProperties(devices[i], properties);
        const auto *props =
            reinterpret_cast<const VkPhysicalDevicePropertiesPrefix *>(properties);

        size_t nameLength = 0;
        while (nameLength < sizeof(props->deviceName)
               && props->deviceName[nameLength] != '\0') {
            ++nameLength;
        }
        const QString name =
            QString::fromUtf8(props->deviceName, int(nameLength));
        if (props->deviceType == VK_PHYSICAL_DEVICE_TYPE_CPU
            || isSoftwareDevice(name)) {
            continue;
        }
        if (!hasGraphicsQueue(devices[i], getQueueFamilies))
            continue;

        if (result->firstRealIndex < 0) {
            result->firstRealIndex = int(i);
            result->firstRealName = name;
        }

        const QString id = pciIdString(props->vendorID, props->deviceID);
        if (!desktopId.isEmpty() && id == desktopId) {
            result->desktopIndex = int(i);
            result->desktopName = name;
            result->ok = true;
            return true;
        }
        if (desktopId.isEmpty() && result->firstRealIndex >= 0) {
            result->ok = true;
            return true;
        }
    }

    result->ok = result->firstRealIndex >= 0;
    return result->ok;
}

} // namespace vk
#endif // UOS_AI_LIBRE_HAS_DLOPEN

bool probeVulkan(const QString &desktopId, bool *surfaceOrder,
                 int *firstRealIndex, int *desktopIndex, QString *firstRealName,
                 QString *desktopName) {
#ifdef UOS_AI_LIBRE_HAS_DLOPEN
    vk::ProbeResult result;
    if (!vk::probe(desktopId, &result))
        return false;
    if (surfaceOrder)
        *surfaceOrder = result.surfaceOrder;
    if (firstRealIndex)
        *firstRealIndex = result.firstRealIndex;
    if (desktopIndex)
        *desktopIndex = result.desktopIndex;
    if (firstRealName)
        *firstRealName = result.firstRealName;
    if (desktopName)
        *desktopName = result.desktopName;
    return result.ok;
#else
    Q_UNUSED(desktopId);
    Q_UNUSED(surfaceOrder);
    Q_UNUSED(firstRealIndex);
    Q_UNUSED(desktopIndex);
    Q_UNUSED(firstRealName);
    Q_UNUSED(desktopName);
    return false;
#endif
}

bool hasOpenGL() {
    QOpenGLContext context;
    context.setFormat(QSurfaceFormat::defaultFormat());
    if (!context.create())
        return false;

    QOffscreenSurface surface;
    surface.setFormat(context.format());
    surface.create();
    if (!surface.isValid())
        return false;

    const bool current = context.makeCurrent(&surface);
    if (current)
        context.doneCurrent();
    return current;
}

QString explicitQtBackend() {
    QStringList values;
    const auto append = [&values](const char *name, const QByteArray &value) {
        if (!value.isEmpty())
            values.append(QStringLiteral("%1=%2")
                              .arg(QLatin1String(name),
                                   QString::fromLocal8Bit(value)));
    };
    append("QSG_RHI_BACKEND", qgetenv("QSG_RHI_BACKEND"));
    append("QT_QUICK_BACKEND", qgetenv("QT_QUICK_BACKEND"));
    append("QT_OPENGL", qgetenv("QT_OPENGL"));
    return values.join(QStringLiteral(", "));
}

} // namespace

namespace RhiBackendSelector {

QSGRendererInterface::GraphicsApi select(QString *detail) {
    const auto setDetail = [detail](const QString &value) {
        if (detail)
            *detail = value;
    };

    const QString forced =
        qEnvironmentVariable("UOS_AI_LIBRE_RHI").trimmed().toLower();
    const bool userIndexSelection = qEnvironmentVariableIsSet("QT_VK_PHYSICAL_DEVICE_INDEX");
    const bool userLoaderSelection = !qEnvironmentVariable("VK_LOADER_DEVICE_SELECT").trimmed().isEmpty();
    const QString desktopId = (userIndexSelection || userLoaderSelection)
                                  ? QString()
                                  : desktopGpuId();

    const auto setDeviceIndex = [&](int index) {
        if (userIndexSelection || userLoaderSelection || index < 0)
            return;
        qputenv("QT_VK_PHYSICAL_DEVICE_INDEX", QByteArray::number(index));
    };

    if (!forced.isEmpty() && forced != QStringLiteral("auto")) {
        if (forced == QStringLiteral("vulkan")) {
            bool surfaceOrder = false;
            int firstRealIndex = -1;
            int desktopIndex = -1;
            QString firstRealName;
            QString desktopName;
            const bool probeOk = probeVulkan(desktopId, &surfaceOrder, &firstRealIndex,
                                              &desktopIndex, &firstRealName,
                                              &desktopName);
            if (probeOk && surfaceOrder) {
                if (desktopId.isEmpty())
                    setDeviceIndex(firstRealIndex);
                else if (desktopIndex >= 0)
                    setDeviceIndex(desktopIndex);
            }
            QString label = QStringLiteral("Vulkan (forced by UOS_AI_LIBRE_RHI");
            const QString name = desktopName.isEmpty() ? firstRealName : desktopName;
            if (!name.isEmpty())
                label += QStringLiteral(", %1").arg(name);
            if (!desktopId.isEmpty())
                label += QStringLiteral(", desktop GPU %1").arg(desktopId);
            label += QLatin1Char(')');
            setDetail(label);
            return QSGRendererInterface::Vulkan;
        }
        if (forced == QStringLiteral("opengl")) {
            setDetail(QStringLiteral("OpenGL (forced by UOS_AI_LIBRE_RHI)"));
            return QSGRendererInterface::OpenGL;
        }
        if (forced == QStringLiteral("software")) {
            setDetail(QStringLiteral("software (forced by UOS_AI_LIBRE_RHI)"));
            return QSGRendererInterface::Software;
        }
        qWarning() << "unknown UOS_AI_LIBRE_RHI value:" << forced
                   << "(expected vulkan, opengl, software or auto)";
    }

    if (const QString qtBackend = explicitQtBackend(); !qtBackend.isEmpty()) {
        setDetail(QStringLiteral("unchanged (%1)").arg(qtBackend));
        return QSGRendererInterface::Unknown;
    }

    const QString platform = QGuiApplication::platformName();
    if (platform == QStringLiteral("offscreen")
        || platform == QStringLiteral("minimal")) {
        setDetail(QStringLiteral("software (no display platform)"));
        return QSGRendererInterface::Software;
    }

    bool surfaceOrder = false;
    int firstRealIndex = -1;
    int desktopIndex = -1;
    QString firstRealName;
    QString desktopName;
    if (probeVulkan(desktopId, &surfaceOrder, &firstRealIndex, &desktopIndex,
                    &firstRealName, &desktopName)) {
        const bool desktopOk = desktopId.isEmpty() || desktopIndex >= 0;
        const bool canPin = desktopId.isEmpty() ? surfaceOrder : (surfaceOrder && desktopOk);
        if (canPin) {
            // Device order only matches QRhi when the platform surface
            // extension was active during enumeration.
            setDeviceIndex(desktopId.isEmpty() ? firstRealIndex : desktopIndex);

            const QString selectedName =
                desktopName.isEmpty() ? firstRealName : desktopName;
            QString label = QStringLiteral("Vulkan");
            if (userIndexSelection) {
                label += QStringLiteral(" (user device index %1)")
                             .arg(qEnvironmentVariable("QT_VK_PHYSICAL_DEVICE_INDEX"));
            } else if (userLoaderSelection) {
                label += QStringLiteral(" (user VK_LOADER_DEVICE_SELECT)");
            } else if (!selectedName.isEmpty()) {
                label += QStringLiteral(" (%1)").arg(selectedName);
            }
            if (!desktopId.isEmpty())
                label += QStringLiteral(", desktop GPU %1").arg(desktopId);
            setDetail(label);
            return QSGRendererInterface::Vulkan;
        }
        if (desktopId.isEmpty()) {
            qWarning() << "Vulkan surface enumeration unavailable; using Qt's default device";
            QString label = QStringLiteral("Vulkan");
            if (!firstRealName.isEmpty())
                label += QStringLiteral(" (%1)").arg(firstRealName);
            setDetail(label);
            return QSGRendererInterface::Vulkan;
        }
        qWarning().noquote()
            << "desktop GPU" << desktopId
            << "is not a usable Vulkan device; falling back to OpenGL";
    }

    if (hasOpenGL()) {
        if (!desktopId.isEmpty())
            setDetail(QStringLiteral("OpenGL (desktop GPU %1)").arg(desktopId));
        else
            setDetail(QStringLiteral("OpenGL"));
        return QSGRendererInterface::OpenGL;
    }

    setDetail(QStringLiteral("software (no graphics device)"));
    return QSGRendererInterface::Software;
}

} // namespace RhiBackendSelector
