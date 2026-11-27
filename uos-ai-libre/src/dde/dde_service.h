#pragma once

#include <QDBusConnection>
#include <QMap>
#include <QObject>
#include <QPoint>
#include <QStringList>

class CopilotAdaptor;
class CopilotChatAdaptor;
class DdeRootAdaptor;
class AiMainAdaptor;
class AiTtsAdaptor;
class AiIatAdaptor;
class AiTransAdaptor;

/// Transport-only compatibility facade for UOS AI's desktop D-Bus API.
///
/// This class deliberately knows nothing about QML or aish. It validates
/// incoming payloads, exposes capability truthfully, and emits intent signals.
/// main.cpp connects those signals to AppController/WindowHost. That keeps the
/// D-Bus contract testable without starting a GUI.
class DdeService final : public QObject {
    Q_OBJECT

public:
    explicit DdeService(QObject *parent = nullptr);
    ~DdeService() override;

    bool start(QString *error);
    void stop();
    bool isStarted() const;

    QString version() const;
    bool isCopilotEnabled() const;
    bool translationAvailable() const;
    void setTranslationAvailable(bool available);

    bool requestQuickImage(int mode, const QString &rawImage,
                           const QPoint &anchorPoint, QString *error);
    bool requestChatImage(const QString &rawImage, QString *error);
    bool requestPrompt(const QString &question,
                       const QMap<QString, QString> &params, QString *error);
    bool requestKnowledgeBase(const QStringList &files, QString *error);

    void requestChatPage(int index);
    void requestChatPageWithToken(int index, const QString &token);
    void requestSettings(bool openAddAccountDialog);
    void requestTranslation();
    void requestScreenshot();
    void requestWordWizard();

signals:
    void chatPageRequested(int index);
    void chatPageWithTokenRequested(int index, const QString &token);
    void chatImageRequested(const QString &imagePath);
    void quickImageRequested(int mode, const QString &imagePath,
                             const QPoint &anchorPoint);
    void settingsRequested(bool openAddAccountDialog);
    void promptRequested(const QString &question, const QString &file,
                         const QString &defaultPrompt);
    void translationRequested();
    void screenshotRequested();
    void wordWizardRequested();

private:
    bool m_started = false;
    bool m_translation_available = false;
    QDBusConnection m_bus;
    DdeRootAdaptor *m_root = nullptr;
    CopilotAdaptor *m_copilot = nullptr;
    CopilotChatAdaptor *m_chat = nullptr;
    AiMainAdaptor *m_ai_main = nullptr;
    AiTtsAdaptor *m_ai_tts = nullptr;
    AiIatAdaptor *m_ai_iat = nullptr;
    AiTransAdaptor *m_ai_trans = nullptr;
};
