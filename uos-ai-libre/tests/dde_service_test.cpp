#include "dde/dde_service.h"

#include <QDBusConnection>
#include <QDBusMessage>
#include <QColor>
#include <QCoreApplication>
#include <QFile>
#include <QGuiApplication>
#include <QImage>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QPoint>
#include <iostream>

namespace {

int fail(const char *message) {
    std::cerr << "dde-service-test: " << message << '\n';
    return 1;
}

QDBusMessage call(const QString &path, const QString &interface,
                  const QString &method, const QVariantList &arguments = {}) {
    QDBusMessage message = QDBusMessage::createMethodCall(
        QStringLiteral("com.deepin.copilot"), path, interface, method);
    message.setArguments(arguments);
    return QDBusConnection::sessionBus().call(message, QDBus::Block, 5000);
}

bool waitForSignal(int *counter) {
    for (int i = 0; i < 10; ++i) {
        QCoreApplication::processEvents();
        if (*counter > 0)
            return true;
    }
    return false;
}

} // namespace

int main(int argc, char *argv[]) {
    QGuiApplication app(argc, argv);
    QStandardPaths::setTestModeEnabled(true);

    DdeService service;
    QString error;
    if (!service.start(&error)) {
        std::cerr << "dde-service-test: start failed: "
                  << error.toStdString() << '\n';
        return 1;
    }

    int chatPages = 0;
    int chatImages = 0;
    int quickImages = 0;
    int prompts = 0;
    int translations = 0;
    int tokenPages = 0;
    QString lastFile;
    QString lastPrompt;
    QObject::connect(&service, &DdeService::chatPageRequested, [&](int) {
        ++chatPages;
    });
    QObject::connect(&service, &DdeService::chatPageWithTokenRequested,
                     [&](int, const QString &) { ++tokenPages; });
    QObject::connect(&service, &DdeService::chatImageRequested,
                     [&](const QString &path) {
                         ++chatImages;
                         if (!QFile::exists(path))
                             std::cerr << "chat image missing\n";
                     });
    QObject::connect(&service, &DdeService::quickImageRequested,
                     [&](int, const QString &path) {
                         ++quickImages;
                         if (!QFile::exists(path))
                             std::cerr << "quick image missing\n";
                     });
    QObject::connect(&service, &DdeService::promptRequested,
                     [&](const QString &, const QString &file,
                         const QString &defaultPrompt) {
                         ++prompts;
                         lastFile = file;
                         lastPrompt = defaultPrompt;
                     });
    QObject::connect(&service, &DdeService::translationRequested,
                     [&]() { ++translations; });

    QDBusMessage reply = call(QStringLiteral("/com/deepin/copilot"),
                              QStringLiteral("com.deepin.copilot"),
                              QStringLiteral("version"));
    if (reply.type() == QDBusMessage::ErrorMessage
        || reply.arguments().value(0).toString() != QStringLiteral("3.0"))
        return fail("version");

    reply = call(QStringLiteral("/"),
                 QStringLiteral("org.freedesktop.DBus.Peer"),
                 QStringLiteral("Ping"));
    if (reply.type() == QDBusMessage::ErrorMessage)
        return fail("Peer.Ping at service root");

    reply = call(QStringLiteral("/com/deepin/copilot"),
                 QStringLiteral("com.deepin.copilot"),
                 QStringLiteral("isCopilotEnabled"));
    if (reply.arguments().value(0).toBool() != true)
        return fail("isCopilotEnabled");

    reply = call(QStringLiteral("/com/deepin/copilot"),
                 QStringLiteral("org.freedesktop.DBus.Introspectable"),
                 QStringLiteral("Introspect"));
    const QString xml = reply.arguments().value(0).toString();
    if (!xml.contains(QStringLiteral("launchAiQuickOCR"))
        || !xml.contains(QStringLiteral("launchChatUploadImage")))
        return fail("screen recorder introspection");

    QTemporaryDir dir;
    if (!dir.isValid())
        return fail("temporary directory");
    QImage image(32, 20, QImage::Format_ARGB32);
    image.fill(QColor(20, 120, 220));
    const QString imagePath = dir.filePath(QStringLiteral("shot.png"));
    if (!image.save(imagePath, "PNG"))
        return fail("image save");

    reply = call(QStringLiteral("/com/deepin/copilot"),
                 QStringLiteral("com.deepin.copilot"),
                 QStringLiteral("launchChatUploadImage"),
                 {imagePath});
    if (reply.type() == QDBusMessage::ErrorMessage || !waitForSignal(&chatImages))
        return fail("launchChatUploadImage signal");

    reply = call(QStringLiteral("/com/deepin/copilot"),
                 QStringLiteral("com.deepin.copilot"),
                 QStringLiteral("launchAiQuickOCR"),
                 {2, QString(), QPoint(30, 40), false, imagePath});
    if (reply.type() == QDBusMessage::ErrorMessage || !waitForSignal(&quickImages))
        return fail("launchAiQuickOCR signal");

    QFile imageFile(imagePath);
    if (!imageFile.open(QIODevice::ReadOnly))
        return fail("read image");
    const QString compressed = QString::fromLatin1(
        qCompress(imageFile.readAll(), 9).toBase64());
    reply = call(QStringLiteral("/com/deepin/copilot"),
                 QStringLiteral("com.deepin.copilot"),
                 QStringLiteral("launchAiQuickOCR"),
                 {1, QString(), QPoint(30, 40), false, compressed});
    if (reply.type() == QDBusMessage::ErrorMessage
        || quickImages < 2)
        return fail("compressed base64 quick image");

    reply = call(QStringLiteral("/com/deepin/copilot"),
                 QStringLiteral("com.deepin.copilot"),
                 QStringLiteral("launchChatUploadImage"),
                 {dir.filePath(QStringLiteral("missing.png"))});
    if (reply.type() != QDBusMessage::ErrorMessage)
        return fail("missing image must fail");

    reply = call(QStringLiteral("/com/deepin/copilot"),
                 QStringLiteral("com.deepin.copilot"),
                 QStringLiteral("launchAiQuickOCR"),
                 {99, QString(), QPoint(0, 0), false, imagePath});
    if (reply.type() != QDBusMessage::ErrorMessage)
        return fail("unknown quick mode must fail");

    const QString textPath = dir.filePath(QStringLiteral("doc.txt"));
    QFile textFile(textPath);
    if (!textFile.open(QIODevice::WriteOnly))
        return fail("text file");
    textFile.write("UOS AI Libre DDE integration");
    textFile.close();
    QMap<QString, QString> params;
    params.insert(QStringLiteral("file"), textPath);
    params.insert(QStringLiteral("defaultPrompt"), QStringLiteral("总结"));
    reply = call(QStringLiteral("/org/deepin/copilot/chat"),
                 QStringLiteral("org.deepin.copilot.chat"),
                 QStringLiteral("inputPrompt"),
                 {QString(), QVariant::fromValue(params)});
    if (reply.type() == QDBusMessage::ErrorMessage || !waitForSignal(&prompts)
        || lastFile != textPath || lastPrompt != QStringLiteral("总结"))
        return fail("inputPrompt");

    QMap<QString, QString> missingParams;
    missingParams.insert(QStringLiteral("file"),
                         dir.filePath(QStringLiteral("missing.txt")));
    reply = call(QStringLiteral("/org/deepin/copilot/chat"),
                 QStringLiteral("org.deepin.copilot.chat"),
                 QStringLiteral("inputPrompt"),
                 {QString(), QVariant::fromValue(missingParams)});
    if (reply.type() != QDBusMessage::ErrorMessage)
        return fail("missing inputPrompt file must fail");

    reply = call(QStringLiteral("/org/deepin/copilot/chat"),
                 QStringLiteral("org.deepin.copilot.chat"),
                 QStringLiteral("sendToKnowledgeBase"),
                 {QStringList{textPath}});
    if (reply.type() != QDBusMessage::ErrorMessage)
        return fail("unsupported knowledge base must fail");

    reply = call(QStringLiteral("/aiassistant/tts"),
                 QStringLiteral("com.iflytek.aiassistant.tts"),
                 QStringLiteral("getTTSEnable"));
    if (reply.arguments().value(0).toBool() != false)
        return fail("TTS capability");
    reply = call(QStringLiteral("/aiassistant/iat"),
                 QStringLiteral("com.iflytek.aiassistant.iat"),
                 QStringLiteral("getIatEnable"));
    if (reply.arguments().value(0).toBool() != false)
        return fail("IAT capability");
    service.setTranslationAvailable(true);
    reply = call(QStringLiteral("/aiassistant/trans"),
                 QStringLiteral("com.iflytek.aiassistant.trans"),
                 QStringLiteral("getTransEnable"));
    if (reply.arguments().value(0).toBool() != true)
        return fail("translation capability");
    reply = call(QStringLiteral("/aiassistant/deepinmain"),
                 QStringLiteral("com.iflytek.aiassistant.mainWindow"),
                 QStringLiteral("TextToTranslate"));
    if (reply.type() == QDBusMessage::ErrorMessage
        || !waitForSignal(&translations))
        return fail("TextToTranslate signal");
    service.setTranslationAvailable(false);
    reply = call(QStringLiteral("/aiassistant/deepinmain"),
                 QStringLiteral("com.iflytek.aiassistant.mainWindow"),
                 QStringLiteral("TextToTranslate"));
    if (reply.type() != QDBusMessage::ErrorMessage)
        return fail("TextToTranslate without capability must fail");

    call(QStringLiteral("/com/deepin/copilot"),
         QStringLiteral("com.deepin.copilot"),
         QStringLiteral("launchChatPage"));
    if (!waitForSignal(&chatPages))
        return fail("launchChatPage signal");
    call(QStringLiteral("/com/deepin/copilot"),
         QStringLiteral("com.deepin.copilot"),
         QStringLiteral("launchChatPageWithToken"),
         {1, QStringLiteral("token")});
    if (!waitForSignal(&tokenPages))
        return fail("launchChatPageWithToken signal");

    DdeService duplicate;
    QString duplicateError;
    if (duplicate.start(&duplicateError))
        return fail("duplicate service registration must fail");

    service.stop();
    std::cout << "dde-service-test: ok\n";
    return 0;
}
