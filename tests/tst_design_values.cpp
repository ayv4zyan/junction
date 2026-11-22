#include "CanvasPainter.h"
#include "DesignValues.h"

#include <QFile>
#include <QGuiApplication>
#include <QJSEngine>
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

    // A JSON Canvas color ("", "1".."6", "#rrggbb") is resolved by CanvasColors.js for the QML
    // items and by Palette::resolve for painting and export; both must give the same color.
    void colorsResolveTheSameInQmlAndCpp()
    {
        QFile file(QStringLiteral(CANVAS_COLORS_JS));
        QVERIFY(file.open(QIODevice::ReadOnly));
        QString script = QString::fromUtf8(file.readAll());
        script.remove(".pragma library");
        QJSEngine js;
        QVERIFY(!js.evaluate(script).isError());

        const Palette palette = *Palette::fromVariantMap(token("Colors", "forPainter").toMap());
        QJSValue presets = js.newArray(palette.presets.size());
        for (int i = 0; i < palette.presets.size(); ++i)
            presets.setProperty(i, palette.presets[i].name());
        const QColor fallback("#888888");
        for (const QString &color : {"", "1", "6", "7", "0", "#123456", "nonsense"}) {
            const QJSValue fromJs = js.globalObject().property("resolve").call({color, presets, fallback.name()});
            QCOMPARE(QColor(fromJs.toString()), palette.resolve(color, fallback));
        }
    }
};

QTEST_MAIN(TestDesignValues)
#include "tst_design_values.moc"
