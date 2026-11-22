#include "JunctionDesign.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(JunctionDesignPlugin)

int main(int argc, char *argv[])
{
    QGuiApplication app(argc, argv);
    JunctionDesign::installFonts();
    QQuickStyle::setStyle("JunctionDesign");

    QQmlApplicationEngine engine;
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
                     [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("JunctionGallery", "Gallery");
    return app.exec();
}
