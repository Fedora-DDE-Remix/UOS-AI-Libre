#pragma once

#include <QLocalServer>
#include <QObject>
#include <QString>

/// Keeps a single UI process per user session.
///
/// A second launch writes `show` to the first instance's local socket and
/// exits, so reopening the app after closing the window brings the existing
/// window (and the still-running aish backend) back instead of starting a
/// second agent.
class SingleInstance final : public QObject {
    Q_OBJECT

public:
    explicit SingleInstance(QObject *parent = nullptr);
    bool isPrimary() const;

signals:
    void showRequested();

private:
    static QString socketName();
    void acceptConnection();

    QLocalServer m_server;
    bool m_primary = true;
};
