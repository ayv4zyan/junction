#include "DesignCheck.h"

#include <QTest>

// The design check: code in the app that goes around the design system (src/design) is
// rejected, so `cmake --build` fails on it (src/app/CMakeLists.txt runs the check).
class TestDesignCheck : public QObject
{
    Q_OBJECT

private slots:
    void rejectsWhatBypassesTheDesignSystem_data()
    {
        QTest::addColumn<QString>("line");
        QTest::addColumn<QString>("useInstead");
        QTest::newRow("hex color") << R"(color: "#7f6df2")" << "Colors";
        QTest::newRow("hex color, single quotes") << R"(color: '#fff')" << "Colors";
        QTest::newRow("color function") << "color: Qt.rgba(1, 0, 0, 1)" << "Colors";
        QTest::newRow("named color") << R"(border.color: "red")" << "Colors";
        QTest::newRow("named color in a function") << R"(color: Qt.alpha("black", 0.5))" << "Colors";
        QTest::newRow("pixel size") << "font.pixelSize: 15" << "Fonts";
        QTest::newRow("point size") << "font.pointSize: Fonts.body" << "Fonts";
        QTest::newRow("font family") << R"(font.family: "Arial")" << "Fonts";
        QTest::newRow("duration") << "NumberAnimation { duration: 120 }" << "Motion";
        QTest::newRow("tooltip delay") << "ToolTip.delay: 600" << "Motion";
        QTest::newRow("easing curve") << "easing.type: Easing.OutBounce" << "Motion";
        QTest::newRow("rounded corner") << "radius: 6" << "Sizes.radius";
        QTest::newRow("rounded corner, fraction") << "radius: 1.5" << "Sizes.radius";
        QTest::newRow("another controls style") << "import QtQuick.Controls.Basic" << "JunctionDesign";
        QTest::newRow("bare templates") << "import QtQuick.Templates as T" << "JunctionDesign";
    }

    void rejectsWhatBypassesTheDesignSystem()
    {
        QFETCH(QString, line);
        QFETCH(QString, useInstead);
        const QList<DesignCheck::Violation> found = DesignCheck::check(line);
        QCOMPARE(found.size(), 1);
        QCOMPARE(found.first().line, 1);
        QVERIFY2(found.first().useInstead.contains(useInstead), qPrintable(found.first().useInstead));
    }

    void acceptsTheDesignSystem_data()
    {
        QTest::addColumn<QString>("line");
        QTest::newRow("token color") << "color: Colors.text";
        QTest::newRow("transparent") << R"(color: "transparent")";
        QTest::newRow("a color's name as text") << R"(tip: qsTr("Red"))";
        QTest::newRow("token size") << "font.pixelSize: Fonts.body";
        QTest::newRow("token timing") << "duration: Motion.fast; easing.type: Motion.enter";
        QTest::newRow("square") << "radius: Sizes.radius";
        QTest::newRow("a computed radius") << "radius: r";
        QTest::newRow("hairline") << "border.width: 1.5 / canvas.zoom";
        QTest::newRow("the styled controls") << "import QtQuick.Controls";
        QTest::newRow("controls helpers") << "import QtQuick.Controls.impl";
        QTest::newRow("a comment") << R"(width: 3   // was "#ff0000", radius: 6)";
        QTest::newRow("a marked exception") << "radius: 4   // design-exception: the OS draws this corner";
    }

    void acceptsTheDesignSystem()
    {
        QFETCH(QString, line);
        const QList<DesignCheck::Violation> found = DesignCheck::check(line);
        QVERIFY2(found.isEmpty(), found.isEmpty() ? "" : qPrintable(found.first().useInstead));
    }

    void anExceptionNeedsAReason()
    {
        QCOMPARE(DesignCheck::check("radius: 4   // design-exception:").size(), 1);
    }

    void reportsLineNumbers()
    {
        const auto found = DesignCheck::check("Item {\n    width: 1\n    font.pixelSize: 15\n}\n");
        QCOMPARE(found.size(), 1);
        QCOMPARE(found.first().line, 3);
    }
};

QTEST_GUILESS_MAIN(TestDesignCheck)
#include "tst_design_check.moc"
