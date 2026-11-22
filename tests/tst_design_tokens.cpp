#include <QDir>
#include <QFile>
#include <QRegularExpression>
#include <QTest>

// The app's QML takes colors, font sizes and animation durations from the design system's
// tokens (src/design), never as typed-in values: those are what drift apart file by file.
// Spacing isn't checked; values like `width: 1` or `1.5 / canvas.zoom` are legitimate.
class TestDesignTokens : public QObject
{
    Q_OBJECT

    // "file:line: text" for every line of the app's QML that matches `pattern`.
    static QStringList offences(const QRegularExpression &pattern)
    {
        QStringList found;
        const QDir dir(QStringLiteral(JUNCTION_APP_QML_DIR));
        for (const QString &name : dir.entryList({QStringLiteral("*.qml")}, QDir::Files)) {
            QFile file(dir.filePath(name));
            if (!file.open(QIODevice::ReadOnly | QIODevice::Text))
                continue;
            int number = 0;
            while (!file.atEnd()) {
                ++number;
                const QString line = QString::fromUtf8(file.readLine()).trimmed();
                const QString code = line.section(QStringLiteral("//"), 0, 0);   // comments may mention values
                if (code.contains(pattern))
                    found.append(QStringLiteral("%1:%2: %3").arg(name).arg(number).arg(line));
            }
        }
        return found;
    }

    static void verifyNone(const QRegularExpression &pattern, const char *useInstead)
    {
        const QStringList found = offences(pattern);
        if (!found.isEmpty())
            QFAIL(qPrintable(QStringLiteral("use %1 instead:\n  ").arg(QLatin1String(useInstead))
                             + found.join(QStringLiteral("\n  "))));
    }

private slots:
    void scansTheAppQml()
    {
        QVERIFY(!QDir(QStringLiteral(JUNCTION_APP_QML_DIR)).entryList({QStringLiteral("*.qml")}).isEmpty());
    }

    void noHexColors() { verifyNone(QRegularExpression(QStringLiteral("\"#[0-9a-fA-F]{3,8}\"")), "Colors"); }

    void noFontSizes() { verifyNone(QRegularExpression(QStringLiteral("pixelSize\\s*:\\s*[0-9]")), "Fonts"); }

    void noDurations() { verifyNone(QRegularExpression(QStringLiteral("duration\\s*:\\s*[0-9]")), "Motion"); }
};

QTEST_GUILESS_MAIN(TestDesignTokens)
#include "tst_design_tokens.moc"
