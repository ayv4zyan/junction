#pragma once

// One running app per Library: two would overwrite each other's index. A second launch hands
// the launcher's activation token to the first one (Wayland only lets a window take focus
// with such a token), which then brings its window forward, and quits.

#include <QLocalServer>
#include <QObject>

class SingleInstance : public QObject
{
    Q_OBJECT

public:
    // `key` names what may be open only once: the Library folder.
    explicit SingleInstance(const QString &key, QObject *parent = nullptr);

    // Asks an app already running for the same key to come forward. True if there was one:
    // this launch should then quit.
    bool handOverToRunning();
    // Waits for later launches, from now on.
    void listen();

signals:
    void activated(const QByteArray &activationToken);   // a later launch; the token may be empty

private:
    QString m_serverName;
    QLocalServer m_server;
};
