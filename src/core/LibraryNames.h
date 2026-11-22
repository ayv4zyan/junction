#pragma once

// The rules for Project and Canvas names. A name is a folder or file name, so some characters
// can't be used, and names in the same place may not differ only in case.

#include <QStringList>

namespace LibraryNames {

bool same(const QString &a, const QString &b);   // ignoring case
// Why `name` can't be used ("" when it can): its characters or length only.
QString problem(const QString &name);
// The same, or `taken` filled in with an `existing` name it equals ignoring case. `except`, the
// item being renamed, doesn't count.
QString problemAmong(const QString &name, const QStringList &existing, const QString &except, const QString &taken);
// `base`, or "base 2", "base 3"... whichever isn't `taken` yet.
QString unique(const QString &base, const QStringList &taken);

} // namespace LibraryNames
