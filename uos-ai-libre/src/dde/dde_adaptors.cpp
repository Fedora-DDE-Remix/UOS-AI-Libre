#include "dde_adaptors.h"

#include "dde_service.h"

#include <QDBusError>
#include <QDebug>
#include <QSysInfo>

DdeRootAdaptor::DdeRootAdaptor(QObject *parent) : QObject(parent) {}

void DdeRootAdaptor::Ping() {}

QString DdeRootAdaptor::GetMachineId() {
    return QString::fromLatin1(QSysInfo::machineUniqueId());
}

CopilotAdaptor::CopilotAdaptor(DdeService *service, QObject *parent)
    : QObject(parent), m_service(service) {}

QString CopilotAdaptor::version() { return m_service->version(); }

QString CopilotAdaptor::registerApp() {
    // The app-specific com.deepin.copilot.app API is a separate integration
    // surface used by browsers and third-party apps. No DDE component in this
    // compatibility scope needs it; return an empty path rather than pretending
    // to register an object that would fail later.
    return {};
}

void CopilotAdaptor::unregisterApp() {}

bool CopilotAdaptor::queryUserExpState() {
    return m_service->isCopilotEnabled();
}

QString CopilotAdaptor::cachedFunctions() { return {}; }

void CopilotAdaptor::launchChatPage() {
    m_service->requestChatPage(0);
}

void CopilotAdaptor::launchChatPage(int index) {
    m_service->requestChatPage(index);
}

void CopilotAdaptor::launchChatPageWithToken(int index, const QString &token) {
    m_service->requestChatPageWithToken(index, token);
}

void CopilotAdaptor::activateChatPage() {
    m_service->requestChatPage(0);
}

void CopilotAdaptor::launchLLMUiPage(bool openAddAccountDialog) {
    m_service->requestSettings(openAddAccountDialog);
}

void CopilotAdaptor::launchWordWizard() { m_service->requestWordWizard(); }

void CopilotAdaptor::textTranslation() { m_service->requestTranslation(); }

void CopilotAdaptor::startScreenshot() { m_service->requestScreenshot(); }

bool CopilotAdaptor::isCopilotEnabled() {
    return m_service->isCopilotEnabled();
}

void CopilotAdaptor::launchAiQuickOCR(int type, const QString &query,
                                       const QPoint &pos, bool isCustom,
                                       const QString &imagePath) {
    Q_UNUSED(query)
    Q_UNUSED(isCustom)
    QString error;
    if (!m_service->requestQuickImage(type, imagePath, pos, &error))
        sendErrorReply(QDBusError::InvalidArgs, error);
}

void CopilotAdaptor::launchChatUploadImage(const QString &imagePath) {
    QString error;
    if (!m_service->requestChatImage(imagePath, &error))
        sendErrorReply(QDBusError::InvalidArgs, error);
}

QStringList CopilotAdaptor::registerAppCmdPrompts(const QVariantMap &cmdPrompts) {
    Q_UNUSED(cmdPrompts)
    return {};
}

void CopilotAdaptor::unregisterAppCmdPrompts() {}

CopilotChatAdaptor::CopilotChatAdaptor(DdeService *service, QObject *parent)
    : QObject(parent), m_service(service) {}

void CopilotChatAdaptor::inputPrompt(
    const QString &question, const QMap<QString, QString> &params) {
    QString error;
    if (!m_service->requestPrompt(question, params, &error))
        sendErrorReply(QDBusError::InvalidArgs, error);
}

void CopilotChatAdaptor::sendToKnowledgeBase(const QStringList &file) {
    QString error;
    if (!m_service->requestKnowledgeBase(file, &error))
        sendErrorReply(QDBusError::NotSupported, error);
}

AiMainAdaptor::AiMainAdaptor(DdeService *service, QObject *parent)
    : QObject(parent), m_service(service) {}

void AiMainAdaptor::TextToSpeech() {
    sendErrorReply(QDBusError::NotSupported,
                   QStringLiteral("TTS is not available in UOS AI Libre"));
}

void AiMainAdaptor::SpeechToText() {
    sendErrorReply(QDBusError::NotSupported,
                   QStringLiteral("STT is not available in UOS AI Libre"));
}

QString AiMainAdaptor::startAsr(const QVariantMap &param) {
    Q_UNUSED(param)
    return QStringLiteral("unsupported");
}

void AiMainAdaptor::stopAsr() {}

void AiMainAdaptor::TextToTranslate() {
    if (!m_service->translationAvailable()) {
        sendErrorReply(QDBusError::NotSupported,
                       QStringLiteral("Translation is not configured"));
        return;
    }
    m_service->requestTranslation();
}

AiTtsAdaptor::AiTtsAdaptor(QObject *parent) : QObject(parent) {}

void AiTtsAdaptor::setTTSEnable(bool enable) { Q_UNUSED(enable) }
bool AiTtsAdaptor::getTTSEnable() { return false; }
void AiTtsAdaptor::setEnableWindow(bool enable) { Q_UNUSED(enable) }
bool AiTtsAdaptor::getEnableWindow() { return false; }
bool AiTtsAdaptor::isTTSInWorking() { return false; }
void AiTtsAdaptor::stopTTSDirectly() {}

AiIatAdaptor::AiIatAdaptor(QObject *parent) : QObject(parent) {}

void AiIatAdaptor::setIatEnable(bool on) { Q_UNUSED(on) }
bool AiIatAdaptor::getIatEnable() { return false; }
bool AiIatAdaptor::setEos(int eos) { Q_UNUSED(eos) return true; }
int AiIatAdaptor::getEos() { return 0; }
bool AiIatAdaptor::setBos(int bos) { Q_UNUSED(bos) return true; }
int AiIatAdaptor::getBos() { return 0; }
bool AiIatAdaptor::setBosWarning(int warningTime) {
    Q_UNUSED(warningTime)
    return true;
}
int AiIatAdaptor::getBosWarning() { return 0; }
void AiIatAdaptor::setIatLanguage(const QString &language) {
    Q_UNUSED(language)
}
QString AiIatAdaptor::getIatLanguage() { return QStringLiteral("zh-CN"); }

AiTransAdaptor::AiTransAdaptor(DdeService *service, QObject *parent)
    : QObject(parent), m_service(service) {}

bool AiTransAdaptor::getTransEnable() {
    return m_service->translationAvailable();
}
