#pragma once

#include <QDBusContext>
#include <QObject>
#include <QPoint>
#include <QStringList>
#include <QVariantMap>

class DdeService;

/// Minimal org.freedesktop.DBus.Peer object at "/".
///
/// DTK probes Peer.Ping before adding AI actions to text controls. A D-Bus
/// connection does not expose Peer methods unless an object exists at the
/// requested path, so this root object is part of the compatibility contract.
class DdeRootAdaptor final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.freedesktop.DBus.Peer")

public:
    explicit DdeRootAdaptor(QObject *parent = nullptr);

public slots:
    Q_SCRIPTABLE void Ping();
    Q_SCRIPTABLE QString GetMachineId();
};

/// com.deepin.copilot at /com/deepin/copilot.
class CopilotAdaptor final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.deepin.copilot")

public:
    explicit CopilotAdaptor(DdeService *service, QObject *parent = nullptr);

public slots:
    Q_SCRIPTABLE QString version();
    Q_SCRIPTABLE QString registerApp();
    Q_SCRIPTABLE void unregisterApp();
    Q_SCRIPTABLE bool queryUserExpState();
    Q_SCRIPTABLE QString cachedFunctions();
    Q_SCRIPTABLE void launchChatPage();
    Q_SCRIPTABLE void launchChatPage(int index);
    Q_SCRIPTABLE void launchChatPageWithToken(int index, const QString &token);
    Q_SCRIPTABLE void activateChatPage();
    Q_SCRIPTABLE void launchLLMUiPage(bool openAddAccountDialog);
    Q_SCRIPTABLE void launchWordWizard();
    Q_SCRIPTABLE void textTranslation();
    Q_SCRIPTABLE void startScreenshot();
    Q_SCRIPTABLE bool isCopilotEnabled();
    Q_SCRIPTABLE void launchAiQuickOCR(int type, const QString &query,
                                        const QPoint &pos, bool isCustom,
                                        const QString &imagePath);
    Q_SCRIPTABLE void launchChatUploadImage(const QString &imagePath);
    Q_SCRIPTABLE QStringList registerAppCmdPrompts(const QVariantMap &cmdPrompts);
    Q_SCRIPTABLE void unregisterAppCmdPrompts();

signals:
    Q_SCRIPTABLE void userExpStateChanged(bool state);
    Q_SCRIPTABLE void windowVisibleChanged(bool visible);
    Q_SCRIPTABLE void windowActiveChanged(bool active);

private:
    DdeService *m_service = nullptr;
};

/// org.deepin.copilot.chat at /org/deepin/copilot/chat.
class CopilotChatAdaptor final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.deepin.copilot.chat")

public:
    explicit CopilotChatAdaptor(DdeService *service,
                                QObject *parent = nullptr);

public slots:
    Q_SCRIPTABLE void inputPrompt(const QString &question,
                                   const QMap<QString, QString> &params);
    Q_SCRIPTABLE void sendToKnowledgeBase(const QStringList &file);

private:
    DdeService *m_service = nullptr;
};

/// com.iflytek.aiassistant.mainWindow at /aiassistant/deepinmain.
class AiMainAdaptor final : public QObject, protected QDBusContext {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.iflytek.aiassistant.mainWindow")

public:
    explicit AiMainAdaptor(DdeService *service, QObject *parent = nullptr);

public slots:
    Q_SCRIPTABLE void TextToSpeech();
    Q_SCRIPTABLE void SpeechToText();
    Q_SCRIPTABLE QString startAsr(const QVariantMap &param);
    Q_SCRIPTABLE void stopAsr();
    Q_SCRIPTABLE void TextToTranslate();

signals:
    Q_SCRIPTABLE void onNotify(const QString &message);

private:
    DdeService *m_service = nullptr;
};

/// com.iflytek.aiassistant.tts at /aiassistant/tts.
class AiTtsAdaptor final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.iflytek.aiassistant.tts")

public:
    explicit AiTtsAdaptor(QObject *parent = nullptr);

public slots:
    Q_SCRIPTABLE void setTTSEnable(bool enable);
    Q_SCRIPTABLE bool getTTSEnable();
    Q_SCRIPTABLE void setEnableWindow(bool enable);
    Q_SCRIPTABLE bool getEnableWindow();
    Q_SCRIPTABLE bool isTTSInWorking();
    Q_SCRIPTABLE void stopTTSDirectly();
};

/// com.iflytek.aiassistant.iat at /aiassistant/iat.
class AiIatAdaptor final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.iflytek.aiassistant.iat")

public:
    explicit AiIatAdaptor(QObject *parent = nullptr);

public slots:
    Q_SCRIPTABLE void setIatEnable(bool on);
    Q_SCRIPTABLE bool getIatEnable();
    Q_SCRIPTABLE bool setEos(int eos);
    Q_SCRIPTABLE int getEos();
    Q_SCRIPTABLE bool setBos(int bos);
    Q_SCRIPTABLE int getBos();
    Q_SCRIPTABLE bool setBosWarning(int warningTime);
    Q_SCRIPTABLE int getBosWarning();
    Q_SCRIPTABLE void setIatLanguage(const QString &language);
    Q_SCRIPTABLE QString getIatLanguage();
};

/// com.iflytek.aiassistant.trans at /aiassistant/trans.
///
/// UOS AI 3.3 does not currently export this object, but DTK-based editors
/// still probe it. We expose it so translation can be capability-advertised
/// independently from TTS/STT.
class AiTransAdaptor final : public QObject {
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "com.iflytek.aiassistant.trans")

public:
    explicit AiTransAdaptor(DdeService *service,
                            QObject *parent = nullptr);

public slots:
    Q_SCRIPTABLE bool getTransEnable();

private:
    DdeService *m_service = nullptr;
};
