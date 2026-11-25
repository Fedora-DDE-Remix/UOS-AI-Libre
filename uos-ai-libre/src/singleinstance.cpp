#include "singleinstance.h"

#include <QDebug>
#include <QDir>
#include <QLocalSocket>
#include <QStandardPaths>

QString SingleInstance::socketName() {
    QString base =
        QStandardPaths::writableLocation(QStandardPaths::RuntimeLocation);
    if (base.isEmpty())
        base = QDir::tempPath();
    return base + QStringLiteral("/uos-ai-libre.sock");
}

SingleInstance::SingleInstance(QObject *parent) : QObject(parent) {
    const QString name = socketName();

    QLocalSocket probe;
    probe.connectToServer(name);
    if (probe.waitForConnected(250)) {
        probe.write("show");
        probe.flush();
        probe.waitForBytesWritten(250);
        m_primary = false;
        return;
    }

    // A stale socket from a crashed process must not block the next launch.
    QLocalServer::removeServer(name);
    if (!m_server.listen(name)) {
        qWarning() << "single-instance socket unavailable:" << m_server.errorString();
        return;
    }
    connect(&m_server, &QLocalServer::newConnection, this,
            &SingleInstance::acceptConnection);
}

bool SingleInstance::isPrimary() const { return m_primary; }

void SingleInstance::acceptConnection() {
    while (QLocalSocket *socket = m_server.nextPendingConnection()) {
        connect(socket, &QLocalSocket::disconnected, socket,
                &QLocalSocket::deleteLater);
        connect(socket, &QLocalSocket::readyRead, this, [this, socket]() {
            const QByteArray data = socket->readAll();
            if (data.contains("show"))
                emit showRequested();
        });
        if (socket->bytesAvailable() > 0) {
            const QByteArray data = socket->readAll();
            if (data.contains("show"))
                emit showRequested();
        }
    }
}
