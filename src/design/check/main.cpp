#include "DesignCheck.h"

#include <QCoreApplication>
#include <QTextStream>

// design-check FOLDER...: lists every place the QML and JS in FOLDER go around the design
// system, and fails (exit code 1) if there is any, which fails the build that runs it.
int main(int argc, char *argv[])
{
	QCoreApplication app(argc, argv);
	QStringList report;
	for (const QString &folder : app.arguments().mid(1))
		report += DesignCheck::checkFolder(folder);

	QTextStream err(stderr);
	for (const QString &line : report)
		err << line << '\n';
	if (!report.isEmpty())
		err << report.size() << " place(s) go around the design system (src/design/README.md). "
			<< "Use the design system, or mark a line `// design-exception: <reason>`.\n";
	return report.isEmpty() ? 0 : 1;
}
