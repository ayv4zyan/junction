#include "LibraryNames.h"

#include <QCoreApplication>
#include <algorithm>

namespace LibraryNames {

namespace {

QString tr(const char *text)
{
	return QCoreApplication::translate("Library", text);
}

} // namespace

bool same(const QString &a, const QString &b)
{
	return a.compare(b, Qt::CaseInsensitive) == 0;
}

QString problem(const QString &name)
{
	if (name.isEmpty())
		return tr("A name can't be empty.");
	if (name.startsWith('.') || name.endsWith('.'))
		return tr("A name can't start or end with a dot.");
	for (const QChar c : name) {
		if (c.category() == QChar::Other_Control || QStringLiteral("/\\:*?\"<>|").contains(c))
			return tr("A name can't contain / \\ : * ? \" < > |");
	}
	if (name.size() > 100)
		return tr("That name is too long.");
	return {};
}

QString problemAmong(const QString &name, const QStringList &existing, const QString &except, const QString &taken)
{
	if (QString characters = problem(name); !characters.isEmpty())
		return characters;
	for (const QString &other : existing) {
		if (other != except && same(other, name))
			return QString(taken).arg(other);
	}
	return {};
}

QString unique(const QString &base, const QStringList &taken)
{
	QString name = base;
	for (int i = 2; std::any_of(taken.begin(), taken.end(), [&](const QString &t) { return same(t, name); }); ++i)
		name = QStringLiteral("%1 %2").arg(base).arg(i);
	return name;
}

} // namespace LibraryNames
