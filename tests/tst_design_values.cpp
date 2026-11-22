#include "CanvasPainter.h"
#include "DesignValues.h"

#include <QGuiApplication>
#include <QQmlEngine>
#include <QTest>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(JunctionDesignPlugin)

// The C++ side needs a few design values without QML (painting, export, hit-testing, where the
// selection toolbar goes). DesignValues.h copies them from the tokens in src/design; this
// fails when the two drift apart.
class TestDesignValues : public QObject
{
    Q_OBJECT

    QQmlEngine m_engine;

    QVariant token(const char *singleton, const char *name)
    {
        QObject *object = m_engine.singletonInstance<QObject *>("JunctionDesign", singleton);
        if (!object)
            qFatal("JunctionDesign.%s can't be loaded", singleton);
        return object->property(name);
    }

private slots:
    void matchTheTokens()
    {
        QCOMPARE(DesignValues::LabelFontSize, token("Fonts", "body").toInt());
        QCOMPARE(DesignValues::CardFontSize, token("Fonts", "canvas").toInt());
        QCOMPARE(DesignValues::CardPadding, token("Space", "md").toReal());
        QCOMPARE(DesignValues::LabelPaddingX, token("Space", "sm").toReal());
        QCOMPARE(DesignValues::LabelPaddingY, token("Space", "xs").toReal());
        QCOMPARE(DesignValues::GroupLabelHeight, token("Sizes", "control").toReal());
        QCOMPARE(DesignValues::GroupLabelSpace, token("Sizes", "control").toReal() + token("Space", "xs").toReal());
        QCOMPARE(DesignValues::GroupTint, token("Colors", "groupTint").toReal());
        QCOMPARE(DesignValues::CardTint, token("Colors", "cardTint").toReal());
        QCOMPARE(DesignValues::GroupLabelTint, token("Colors", "groupLabelTint").toReal());
    }

    // The painted layers and exports get their colors from Colors.forPainter, by key.
    void forPainterGivesACompletePalette()
    {
        QVERIFY(Palette::fromVariantMap(token("Colors", "forPainter").toMap()).has_value());
        QVariantMap incomplete = token("Colors", "forPainter").toMap();
        incomplete.remove("cardFill");
        QVERIFY(!Palette::fromVariantMap(incomplete).has_value());
    }
};

QTEST_MAIN(TestDesignValues)
#include "tst_design_values.moc"
