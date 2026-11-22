#include "DesignCheck.h"

#include <QDir>
#include <QFile>
#include <QRegularExpression>

namespace {

struct Rule
{
    QRegularExpression pattern;
    QString useInstead;
};

// Each rule is one way of going around the design system, with what to use instead.
const QList<Rule> &rules()
{
    static const QList<Rule> list = [] {
        auto rule = [](const char *pattern, const char *useInstead) {
            return Rule{QRegularExpression(QString::fromLatin1(pattern)), QString::fromLatin1(useInstead)};
        };
        return QList<Rule>{
            rule(R"(["']#[0-9a-fA-F]{3,8}["'])", "a color from Colors"),
            rule(R"(\bQt\.(rgba|hsla|hsva)\s*\()", "a color from Colors"),
            rule(R"(\b\w*[cC]olor\s*:\s*["'](?!transparent["'])[a-zA-Z]+["'])", "a color from Colors"),
            rule(R"(\bQt\.(alpha|lighter|darker|tint)\s*\(\s*["'])", "a color from Colors"),
            rule(R"(\bpixelSize\s*:\s*[0-9])", "a size from Fonts"),
            rule(R"(\bpointSize\s*:)", "pixelSize with a size from Fonts"),
            rule(R"(\bfamily\s*:\s*["'])", "the app font (Fonts.family)"),
            rule(R"(\b(duration|delay)\s*:\s*[0-9])", "a time from Motion"),
            rule(R"(\beasing\.type\s*:\s*Easing\.)", "Motion.enter or Motion.exit"),
            rule(R"(\bradius\s*:\s*(0*[1-9][0-9]*(\.[0-9]+)?|0*\.[0-9]*[1-9][0-9]*)\s*(;|\}|$))",
                 "Sizes.radius (square, everywhere)"),
            rule(R"(^\s*import\s+QtQuick\.Controls\.(?!impl\b)\w+)",
                 "plain `import QtQuick.Controls`, which gives the JunctionDesign style"),
            rule(R"(^\s*import\s+QtQuick\.Templates\b)",
                 "plain `import QtQuick.Controls`, which gives the JunctionDesign style"),
        };
    }();
    return list;
}

// A line that opts out says why: "// design-exception: <reason>".
const QRegularExpression exception(QStringLiteral(R"(//\s*design-exception:\s*\S)"));
// A comment to the end of the line (but not the "//" in "qrc://" or "http://").
const QRegularExpression comment(QStringLiteral(R"((?<!:)//.*$)"));

} // namespace

QList<DesignCheck::Violation> DesignCheck::check(const QString &source)
{
    QList<Violation> found;
    const QStringList lines = source.split(QLatin1Char('\n'));
    for (int i = 0; i < lines.size(); ++i) {
        const QString &line = lines[i];
        if (line.contains(exception))
            continue;
        const QString code = QString(line).remove(comment);   // comments may mention any value
        for (const Rule &rule : rules()) {
            if (code.contains(rule.pattern)) {
                found.append({i + 1, line.trimmed(), rule.useInstead});
                break;   // one report per line is enough
            }
        }
    }
    return found;
}

QStringList DesignCheck::checkFolder(const QString &folder)
{
    QStringList report;
    const QDir dir(folder);
    for (const QString &name : dir.entryList({QStringLiteral("*.qml"), QStringLiteral("*.js")}, QDir::Files, QDir::Name)) {
        QFile file(dir.filePath(name));
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            report.append(QStringLiteral("%1: can't be read").arg(file.fileName()));
            continue;
        }
        for (const Violation &v : check(QString::fromUtf8(file.readAll())))
            report.append(QStringLiteral("%1:%2: %3  ->  use %4").arg(file.fileName()).arg(v.line).arg(v.text, v.useInstead));
    }
    return report;
}
