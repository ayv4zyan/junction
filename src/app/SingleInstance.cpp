#include "SingleInstance.h"

#include <QCryptographicHash>
#include <QLocalSocket>

SingleInstance::SingleInstance(const QString &key, QObject *parent)
    : QObject(parent)
{
    const QByteArray hash = QCryptographicHash::hash(key.toUtf8(), QCryptographicHash::Sha1);
    m_serverName = "junction-" + QString::fromLatin1(hash.toHex().left(12));
}

bool SingleInstance::handOverToRunning()
{
    QLocalSocket running;
    running.connectToServer(m_serverName);
    if (!running.waitForConnected(500))
        return false;
    running.write(qgetenv("XDG_ACTIVATION_TOKEN"));
    running.waitForBytesWritten(500);
    return true;
}

void SingleInstance::listen()
{
    QLocalServer::removeServer(m_serverName);   // a socket left behind by a crash
    if (!m_server.listen(m_serverName))
        qWarning("Junction: %s; a second launch will not find this one", qPrintable(m_server.errorString()));
    connect(&m_server, &QLocalServer::newConnection, this, [this] {
        while (QLocalSocket *socket = m_server.nextPendingConnection()) {
            // The token is all a launch sends; it has arrived when the launch hangs up.
            connect(socket, &QLocalSocket::disconnected, this, [this, socket] {
                emit activated(socket->readAll());
                socket->deleteLater();
            });
        }
    });
}
