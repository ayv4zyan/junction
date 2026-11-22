#include "Editor.h"
#include "JunctionDesign.h"

#include <QCryptographicHash>
#include <QGuiApplication>
#include <QLocalServer>
#include <QLocalSocket>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QStyleHints>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(JunctionDesignPlugin)

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName("Junction");
    app.setApplicationName("Junction");
    // JUNCTION_COLOR_SCHEME=light|dark overrides the system setting (to check both themes).
    if (const QByteArray scheme = qgetenv("JUNCTION_COLOR_SCHEME"); !scheme.isEmpty())
        QGuiApplication::styleHints()->setColorScheme(scheme == "light" ? Qt::ColorScheme::Light : Qt::ColorScheme::Dark);
    JunctionDesign::installFonts();
    QQuickStyle::setStyle("JunctionDesign");   // plain Button, Menu, ... get the design system's look

    // One running app per Library: two would overwrite each other's index. A second launch
    // connects to the first, which brings its window forward, and then quits. The message is
    // the launcher's activation token (Wayland only lets a window take focus with one).
    const QString libraryFolder = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/Library";
    const QByteArray libraryHash = QCryptographicHash::hash(libraryFolder.toUtf8(), QCryptographicHash::Sha1);
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
    if (!server.listen(serverName))
        qWarning("Junction: %s; a second launch will not find this one", qPrintable(server.errorString()));

    Editor editor(libraryFolder);   // outlives the engine, which is destroyed first
    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.setInitialProperties({{"editor", QVariant::fromValue(&editor)}});
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
