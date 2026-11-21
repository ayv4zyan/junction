#include "Document.h"
#include "TransactionCommand.h"

#include <QJsonArray>
#include <QTest>
#include <QUndoStack>

class TestDocument : public QObject
{
    Q_OBJECT

    static Node card(const QString &id, QRectF rect, const QString &text = {})
    {
        Node n;
        n.id = id;
        n.rect = rect;
        n.text = text;
        return n;
    }

private slots:
    void addUpdateRemove()
    {
        Document doc;
        const Node a = card("a", {0, 0, 100, 50}, "hello");
        doc.apply(Change{std::nullopt, a});
        QCOMPARE(doc.nodes().size(), 1);

        Node moved = a;
        moved.rect.moveTo(40, 40);
        doc.apply(Change{a, moved});
        QCOMPARE(doc.node("a")->rect.topLeft(), QPointF(40, 40));

        doc.apply(Change{moved, std::nullopt});
        QVERIFY(doc.nodes().isEmpty());
    }

    void undoRedoRestoresEverything()
    {
        Document doc;
        QUndoStack undo;
        const Node a = card("a", {0, 0, 100, 50});
        const Node b = card("b", {200, 0, 100, 50});
        Connection c;
        c.id = "c";
        c.fromNode = "a";
        c.toNode = "b";

        undo.push(new TransactionCommand(&doc, {{std::nullopt, a}, {std::nullopt, b}, {std::nullopt, c}}, "add"));
        QCOMPARE(doc.nodes().size(), 2);
        QCOMPARE(doc.connections().size(), 1);

        // Deleting a card takes its Connection with it, in one step.
        undo.push(new TransactionCommand(&doc, {{c, std::nullopt}, {a, std::nullopt}}, "delete"));
        QCOMPARE(doc.nodes().size(), 1);
        QVERIFY(doc.connections().isEmpty());

        undo.undo();
        QCOMPARE(doc.nodes().size(), 2);
        QCOMPARE(*doc.connection("c"), c);

        undo.undo();
        QVERIFY(doc.nodes().isEmpty());
        undo.redo();
        undo.redo();
        QCOMPARE(doc.nodes().size(), 1);
    }

    void jsonRoundTrip()
    {
        Document doc;
        Node group;
        group.id = "g";
        group.kind = NodeKind::Group;
        group.label = "Backend";
        group.rect = {-20, -20, 400, 200};
        group.color = "4";
        Connection c;
        c.id = "c";
        c.fromNode = "a";
        c.toNode = "b";
        c.toSide = Side::Left;
        c.label = "calls";
        c.style = PathStyle::Straight;
        c.waypoints = {{100, 300}};
        doc.apply(Transaction{{std::nullopt, group},
                              {std::nullopt, card("a", {0, 0, 100, 60}, "# API")},
                              {std::nullopt, card("b", {300, 0, 100, 60}, "DB")},
                              {std::nullopt, c}});

        Document copy;
        copy.fromJson(doc.toJson());
        QCOMPARE(copy.nodes(), doc.nodes());
        QCOMPARE(copy.connections(), doc.connections());
    }

    void unsupportedCardsAreWrittenBackUnchanged()
    {
        const QJsonObject file{{"id", "f"}, {"type", "file"}, {"file", "notes/Plan.md"},
                               {"subpath", "#Goals"}, {"x", 10}, {"y", 20}, {"width", 300}, {"height", 200}};
        Document doc;
        doc.fromJson({{"nodes", QJsonArray{file}}});
        QCOMPARE(doc.node("f")->kind, NodeKind::Unsupported);
        QCOMPARE(doc.toJson()["nodes"].toArray().first().toObject(), file);
    }

    void obsidianSidesBecomeFloating()
    {
        // Written by Obsidian: no "pathStyle", so its sides are not kept.
        const Connection fromObsidian = Connection::fromJson(
            {{"id", "e"}, {"fromNode", "a"}, {"fromSide", "right"}, {"toNode", "b"}, {"toSide", "left"}});
        QCOMPARE(fromObsidian.fromSide, Side::None);
        QCOMPARE(fromObsidian.toSide, Side::None);
        QVERIFY(fromObsidian.toArrow);
        QCOMPARE(fromObsidian.style, PathStyle::Curved);
    }
};

QTEST_GUILESS_MAIN(TestDocument)
#include "tst_document.moc"
