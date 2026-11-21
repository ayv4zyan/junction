#include "Editor.h"

#include <QCryptographicHash>
#include <QGuiApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName("Junction");
    app.setApplicationName("Junction");
    QQuickStyle::setStyle("Fusion");

    // One running app per Library: two would overwrite each other's index. A second launch
    // connects to the first, which brings its window forward, and then quits. The message is
    // the launcher's activation token (Wayland only lets a window take focus with one).
    const QByteArray libraryHash = QCryptographicHash::hash(Editor::libraryFolder().toUtf8(), QCryptographicHash::Sha1);
    const QString serverName = "junction-" + QString::fromLatin1(libraryHash.toHex().left(12));
    {
        QLocalSocket running;
        running.connectToServer(serverName);
        if (running.waitForConnected(500)) {
            running.write(qgetenv("XDG_ACTIVATION_TOKEN"));
            running.waitForBytesWritten(500);
            return 0;
        }
    }
    QLocalServer::removeServer(serverName);   // a socket left behind by a crash
    QLocalServer server;
    server.listen(serverName);

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Junction", "Main");

    QObject::connect(&server, &QLocalServer::newConnection, &app, [&] {
        while (QLocalSocket *socket = server.nextPendingConnection()) {
            QObject::connect(socket, &QLocalSocket::disconnected, socket, [&engine, socket] {
                const QByteArray token = socket->readAll();
                if (!token.isEmpty())
                    qputenv("XDG_ACTIVATION_TOKEN", token);
                if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0))) {
                    window->setWindowStates(window->windowStates() & ~Qt::WindowMinimized);
                    window->show();
                    window->raise();
                    window->requestActivate();
                    window->alert(0);   // where focus can't be taken, at least ask for attention
                }
                socket->deleteLater();
            });
        }
    });
    return app.exec();
}
