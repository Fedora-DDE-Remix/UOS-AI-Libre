#include "ddeshortcutbackend.h"

#include <QDBusArgument>
#include <QDBusConnection>
#include <QDBusConnectionInterface>
#include <QDBusInterface>
#include <QDBusMetaType>
#include <QDBusReply>
#include <QGuiApplication>
#include <QMetaType>
#include <QStringList>

#include <memory>

struct ShortcutInfo {
    QString id;
    QString name;
    QString category;
    QStringList hotkeys;
    QString displayName;
    QString displayNameZh;
    bool custom = false;
    bool modifiable = true;
};

Q_DECLARE_METATYPE(ShortcutInfo)
Q_DECLARE_METATYPE(QList<ShortcutInfo>)

QDBusArgument &operator<<(QDBusArgument &argument, const ShortcutInfo &info) {
    argument.beginStructure();
    argument << info.id << info.name << info.category << info.hotkeys
             << info.displayName << info.displayNameZh << info.custom
             << info.modifiable;
    argument.endStructure();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument,
                                ShortcutInfo &info) {
    argument.beginStructure();
    argument >> info.id >> info.name >> info.category >> info.hotkeys
             >> info.displayName >> info.displayNameZh >> info.custom
             >> info.modifiable;
    argument.endStructure();
    return argument;
}

QDBusArgument &operator<<(QDBusArgument &argument,
                          const QList<ShortcutInfo> &list) {
    argument.beginArray(qMetaTypeId<ShortcutInfo>());
    for (const ShortcutInfo &info : list)
        argument << info;
    argument.endArray();
    return argument;
}

const QDBusArgument &operator>>(const QDBusArgument &argument,
                                QList<ShortcutInfo> &list) {
    argument.beginArray();
    while (!argument.atEnd()) {
        ShortcutInfo info;
        argument >> info;
        list.append(info);
    }
    argument.endArray();
    return argument;
}


namespace {

constexpr const char *kService = "org.deepin.dde.Keybinding1";
constexpr const char *kPath = "/org/deepin/dde/Keybinding1";
constexpr const char *kInterface = "org.deepin.dde.Keybinding1";
constexpr const char *kUosAiShortcut =
    "org.deepin.dde.keybinding.shortcut.app.uos-ai";


void registerMetaTypes() {
    static const bool registered = []() {
        qDBusRegisterMetaType<ShortcutInfo>();
        qDBusRegisterMetaType<QList<ShortcutInfo>>();
        return true;
    }();
    Q_UNUSED(registered)
}

std::unique_ptr<QDBusInterface> keybindingInterface() {
    registerMetaTypes();
    return std::make_unique<QDBusInterface>(
        QString::fromLatin1(kService), QString::fromLatin1(kPath),
        QString::fromLatin1(kInterface), QDBusConnection::sessionBus());
}


QString ddeSequence(const QString &sequence) {
    QStringList parts = sequence.split(QLatin1Char('+'));
    if (parts.isEmpty())
        return sequence;
    QString key = parts.takeLast().trimmed();
    key = key.toLower();
    QString result;
    for (QString part : parts) {
        part = part.trimmed();
        if (part.compare(QStringLiteral("Ctrl"), Qt::CaseInsensitive) == 0)
            part = QStringLiteral("Control");
        else if (part.compare(QStringLiteral("Meta"), Qt::CaseInsensitive) == 0
                 || part.compare(QStringLiteral("Super"), Qt::CaseInsensitive) == 0)
            part = QStringLiteral("Super");
        result += QLatin1Char('<') + part + QLatin1Char('>');
    }
    return result + key;
}

QList<ShortcutInfo> searchShortcuts(const QString &name) {
    auto iface = keybindingInterface();
    if (!iface || !iface->isValid())
        return {};
    const QDBusReply<QList<ShortcutInfo>> reply =
        iface->call(QStringLiteral("SearchShortcuts"), name);
    return reply.isValid() ? reply.value() : QList<ShortcutInfo>{};
}

bool isWaylandSession() {
    const QString platform = QGuiApplication::platformName().toLower();
    return platform.contains(QStringLiteral("wayland"))
           || platform.contains(QStringLiteral("treeland"));
}

} // namespace

QString DdeShortcutBackend::name() const {
    return QStringLiteral("dde-keybinding");
}

bool DdeShortcutBackend::available() const {
    registerMetaTypes();
    QDBusConnectionInterface *bus = QDBusConnection::sessionBus().interface();
    if (!bus)
        return false;
    const QDBusReply<bool> registered =
        bus->isServiceRegistered(QString::fromLatin1(kService));
    return registered.isValid() && registered.value();
}

bool DdeShortcutBackend::registerShortcut(const QString &id,
                                          const QString &sequence,
                                          const QString &command,
                                          QString *error) {
    if (error)
        error->clear();
    if (id.isEmpty() || sequence.isEmpty() || command.isEmpty()) {
        if (error)
            *error = QStringLiteral("快捷键参数为空");
        return false;
    }

    auto iface = keybindingInterface();
    if (!iface || !iface->isValid()) {
        if (error)
            *error = QStringLiteral("无法连接 DDE 快捷键服务");
        return false;
    }

    // Replace our previous binding by name so repeated starts are idempotent.
    for (const ShortcutInfo &existing : searchShortcuts(id)) {
        if (existing.category == QStringLiteral("Custom"))
            iface->call(QStringLiteral("DeleteCustomShortcut"), existing.id);
    }

    const QString dde_sequence = ddeSequence(sequence);
    auto add = [&]() {
        return iface->call(QStringLiteral("AddCustomShortcut"), id, command,
                           dde_sequence);
    };
    QDBusReply<QString> added = add();

    // On a fresh UOS AI session Super+Space is already owned by UOS AI. Only
    // disable that entry when the first registration actually conflicts; this
    // keeps failures from silently removing a working system shortcut.
    if ((!added.isValid() || added.value().isEmpty())
        && id == QStringLiteral("UOS AI Libre")
        && (sequence.contains(QStringLiteral("Meta"))
            || sequence.contains(QStringLiteral("Super")))) {
        iface->call(QStringLiteral("Disable"),
                   QString::fromLatin1(kUosAiShortcut));
        added = add();
    }

    if (!added.isValid() || added.value().isEmpty()) {
        if (error) {
            *error = iface->lastError().message().isEmpty()
                         ? QStringLiteral("注册 DDE 快捷键失败")
                         : iface->lastError().message();
        }
        return false;
    }
    m_customIds.insert(id, added.value());
    return true;
}

bool DdeShortcutBackend::unregisterShortcut(const QString &id, QString *error) {
    if (error)
        error->clear();
    auto iface = keybindingInterface();
    if (!iface || !iface->isValid()) {
        if (error)
            *error = QStringLiteral("无法连接 DDE 快捷键服务");
        return false;
    }

    bool removed = false;
    QString customId = m_customIds.take(id);
    if (customId.isEmpty()) {
        const QList<ShortcutInfo> matches = searchShortcuts(id);
        if (!matches.isEmpty())
            customId = matches.first().id;
    }
    if (!customId.isEmpty()) {
        const QDBusReply<bool> reply =
            iface->call(QStringLiteral("DeleteCustomShortcut"), customId);
        removed = reply.isValid() && reply.value();
    }
    return removed;
}

QString TreelandShortcutBackend::name() const {
    return QStringLiteral("treeland-dde");
}

bool TreelandShortcutBackend::available() const {
    return isWaylandSession() && DdeShortcutBackend::available();
}
