#include "dde_service.h"

#include "dde_adaptors.h"
#include "dde_payload.h"

#include <QDBusConnectionInterface>
#include <QDBusError>
#include <QDBusMetaType>
#include <QDebug>
#include <QFileInfo>

namespace {

constexpr const char *kCopilotService = "com.deepin.copilot";
constexpr const char *kAiService = "com.iflytek.aiassistant";
constexpr const char *kCopilotPath = "/com/deepin/copilot";
constexpr const char *kCopilotChatPath = "/org/deepin/copilot/chat";
constexpr const char *kAiMainPath = "/aiassistant/deepinmain";
constexpr const char *kAiTtsPath = "/aiassistant/tts";
constexpr const char *kAiIatPath = "/aiassistant/iat";
constexpr const char *kAiTransPath = "/aiassistant/trans";

} // namespace

DdeService::DdeService(QObject *parent)
    : QObject(parent), m_bus(QDBusConnection::sessionBus()) {
    qDBusRegisterMetaType<QMap<QString, QString>>();
}

DdeService::~DdeService() { stop(); }

bool DdeService::start(QString *error) {
    if (error)
        error->clear();
    if (m_started)
        return true;
    if (!m_bus.isConnected()) {
        if (error)
            *error = QStringLiteral("无法连接 D-Bus 会话总线");
        return false;
    }

    m_copilot = new CopilotAdaptor(this, this);
    m_chat = new CopilotChatAdaptor(this, this);
    m_root = new DdeRootAdaptor(this);
    m_ai_main = new AiMainAdaptor(this, this);
    m_ai_tts = new AiTtsAdaptor(this);
    m_ai_iat = new AiIatAdaptor(this);
    m_ai_trans = new AiTransAdaptor(this, this);

    if (!m_bus.registerService(QString::fromLatin1(kCopilotService))) {
        if (error)
            *error = QStringLiteral("无法注册 %1：%2")
                         .arg(QString::fromLatin1(kCopilotService),
                              m_bus.lastError().message());
        stop();
        return false;
    }
    if (!m_bus.registerService(QString::fromLatin1(kAiService))) {
        if (error)
            *error = QStringLiteral("无法注册 %1：%2")
                         .arg(QString::fromLatin1(kAiService),
                              m_bus.lastError().message());
        stop();
        return false;
    }

    const auto fail = [this, error](const QString &path) {
        if (error)
            *error = QStringLiteral("无法注册 D-Bus 对象 %1：%2")
                         .arg(path, m_bus.lastError().message());
        stop();
        return false;
    };

    if (!m_bus.registerObject(QStringLiteral("/"), m_root,
                              QDBusConnection::ExportAllSlots))
        return fail(QStringLiteral("/"));
    if (!m_bus.registerObject(QString::fromLatin1(kCopilotPath), m_copilot,
                              QDBusConnection::ExportAllSlots |
                                  QDBusConnection::ExportAllSignals))
        return fail(QString::fromLatin1(kCopilotPath));
    if (!m_bus.registerObject(QString::fromLatin1(kCopilotChatPath), m_chat,
                              QDBusConnection::ExportAllSlots |
                                  QDBusConnection::ExportAllSignals))
        return fail(QString::fromLatin1(kCopilotChatPath));
    if (!m_bus.registerObject(QString::fromLatin1(kAiMainPath), m_ai_main,
                              QDBusConnection::ExportAllSlots |
                                  QDBusConnection::ExportAllSignals))
        return fail(QString::fromLatin1(kAiMainPath));
    if (!m_bus.registerObject(QString::fromLatin1(kAiTtsPath), m_ai_tts,
                              QDBusConnection::ExportAllSlots))
        return fail(QString::fromLatin1(kAiTtsPath));
    if (!m_bus.registerObject(QString::fromLatin1(kAiIatPath), m_ai_iat,
                              QDBusConnection::ExportAllSlots))
        return fail(QString::fromLatin1(kAiIatPath));
    if (!m_bus.registerObject(QString::fromLatin1(kAiTransPath), m_ai_trans,
                              QDBusConnection::ExportAllSlots))
        return fail(QString::fromLatin1(kAiTransPath));

    m_started = true;
    return true;
}

void DdeService::stop() {
    if (m_bus.isConnected()) {
        for (const char *path :
             {"/", kCopilotPath, kCopilotChatPath, kAiMainPath, kAiTtsPath,
              kAiIatPath, kAiTransPath}) {
            m_bus.unregisterObject(QString::fromLatin1(path));
        }
        m_bus.unregisterService(QString::fromLatin1(kAiService));
        m_bus.unregisterService(QString::fromLatin1(kCopilotService));
    }
    delete m_root;
    delete m_copilot;
    delete m_chat;
    delete m_ai_main;
    delete m_ai_tts;
    delete m_ai_iat;
    delete m_ai_trans;
    m_root = nullptr;
    m_copilot = nullptr;
    m_chat = nullptr;
    m_ai_main = nullptr;
    m_ai_tts = nullptr;
    m_ai_iat = nullptr;
    m_ai_trans = nullptr;
    m_started = false;
}

bool DdeService::isStarted() const { return m_started; }

QString DdeService::version() const { return QStringLiteral("3.0"); }

bool DdeService::isCopilotEnabled() const { return true; }

bool DdeService::translationAvailable() const {
    return m_translation_available;
}

void DdeService::setTranslationAvailable(bool available) {
    m_translation_available = available;
}

bool DdeService::requestQuickImage(int mode, const QString &rawImage,
                                   const QPoint &anchorPoint, QString *error) {
    if (mode < 1 || mode > 4) {
        if (error)
            *error = QStringLiteral("未知的 AI 快捷操作类型：%1").arg(mode);
        return false;
    }
    const QString path = DdePayload::normalizeImage(rawImage, error);
    if (path.isEmpty())
        return false;
    emit quickImageRequested(mode, path, anchorPoint);
    return true;
}

bool DdeService::requestChatImage(const QString &rawImage, QString *error) {
    const QString path = DdePayload::normalizeImage(rawImage, error);
    if (path.isEmpty())
        return false;
    emit chatImageRequested(path);
    return true;
}

bool DdeService::requestPrompt(const QString &question,
                               const QMap<QString, QString> &params,
                               QString *error) {
    const QString file = params.value(QStringLiteral("file")).trimmed();
    const QString defaultPrompt =
        params.value(QStringLiteral("defaultPrompt")).trimmed();
    if (question.trimmed().isEmpty() && defaultPrompt.isEmpty()
        && file.isEmpty()) {
        if (error)
            *error = QStringLiteral("提示词和文件不能同时为空");
        return false;
    }
    if (!file.isEmpty() && !QFileInfo(file).isFile()) {
        if (error)
            *error = QStringLiteral("文件不存在：%1").arg(file);
        return false;
    }
    emit promptRequested(question, file, defaultPrompt);
    return true;
}

bool DdeService::requestKnowledgeBase(const QStringList &files, QString *error) {
    Q_UNUSED(files)
    if (error)
        *error = QStringLiteral(
            "知识库功能尚未在 UOS AI Libre 中启用；当前版本不会假装文件已经入库");
    return false;
}

void DdeService::requestChatPage(int index) {
    emit chatPageRequested(index);
}

void DdeService::requestChatPageWithToken(int index, const QString &token) {
    emit chatPageWithTokenRequested(index, token);
}

void DdeService::requestSettings(bool openAddAccountDialog) {
    emit settingsRequested(openAddAccountDialog);
}

void DdeService::requestTranslation() { emit translationRequested(); }

void DdeService::requestScreenshot() { emit screenshotRequested(); }

void DdeService::requestWordWizard() { emit wordWizardRequested(); }
