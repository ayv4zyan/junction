#pragma once

// The design check: finds code in the app that goes around the design system, such as a typed-in
// color, font size, duration or rounded corner, or a different Qt Quick Controls style. The
// app's build runs it (design-check, src/app/CMakeLists.txt), so such code doesn't build.
// A line can opt out with a comment saying why: `// design-exception: <reason>`.

#include <QList>
#include <QString>

namespace DesignCheck {

struct Violation
{
    int line = 0;          // 1-based
    QString text;          // the offending line, trimmed
    QString useInstead;    // what to use instead
};

// The violations in one QML or JavaScript source.
QList<Violation> check(const QString &source);

// "file:line: text  →  use …" for every violation in the .qml and .js files of `folder`.
QStringList checkFolder(const QString &folder);

} // namespace DesignCheck
