#include "Editor.h"
#include "Exporter.h"
#include "HitTest.h"
#include "LibraryRows.h"
#include "SingleInstance.h"
#include "JunctionDesign.h"

#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QStyleHints>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(JunctionDesignPlugin)

// Brings the window forward for a later launch. Qt's Wayland support takes the launch's
// activation token from XDG_ACTIVATION_TOKEN when the window asks to be activated.
static void bringForward(QQuickWindow *window, const QByteArray &activationToken)
{
	if (!activationToken.isEmpty())
		qputenv("XDG_ACTIVATION_TOKEN", activationToken);
	window->setWindowStates(window->windowStates() & ~Qt::WindowMinimized);
	window->show();
	window->raise();
	window->requestActivate();
	window->alert(0);   // where focus can't be taken, at least ask for attention
}

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

	const QString libraryFolder = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/Library";
	SingleInstance instance(libraryFolder);
	if (instance.handOverToRunning())
		return 0;
	instance.listen();

	// These outlive the engine, which is destroyed first.
	Library library(libraryFolder);
	LibraryRows libraryRows(&library);
	Editor editor(&library, QGuiApplication::clipboard());
	HitTest hitTest(&editor.document());
	Exporter exporter(&editor.document());
	QQmlApplicationEngine engine;
	QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed, &app,
					 [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
	engine.setInitialProperties({
		{"library", QVariant::fromValue(&library)},
		{"libraryRows", QVariant::fromValue(&libraryRows)},
		{"editor", QVariant::fromValue(&editor)},
		{"canvasSession", QVariant::fromValue(editor.canvasSession())},
		{"hitTest", QVariant::fromValue(&hitTest)},
		{"exporter", QVariant::fromValue(&exporter)},
	});
	engine.loadFromModule("Junction", "Main");

	QObject::connect(&instance, &SingleInstance::activated, &app, [&engine](const QByteArray &token) {
		if (auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().value(0)))
			bringForward(window, token);
	});
	return app.exec();
}
