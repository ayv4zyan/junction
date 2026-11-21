#include <QFileInfo>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    app.setOrganizationName("Junction");
    app.setApplicationName("Junction");
    QQuickStyle::setStyle("Fusion");

    // `junction path/to/file.canvas` opens that file.
    const QStringList args = app.arguments();
    const QString file = args.size() > 1 ? QFileInfo(args[1]).absoluteFilePath() : QString();

    QQmlApplicationEngine engine;
    engine.setInitialProperties({{"initialFile", file}});
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("Junction", "Main");
    return app.exec();
}
