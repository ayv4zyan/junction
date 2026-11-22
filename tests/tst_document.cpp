#include "Document.h"
#include "TransactionCommand.h"

#include <QFile>
#include <QJsonArray>
#include <QTemporaryDir>
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
        QCOMPARE(fromObsidian.fromSide, Side::Floating);
        QCOMPARE(fromObsidian.toSide, Side::Floating);
        QVERIFY(fromObsidian.toArrow);
        QCOMPARE(fromObsidian.style, PathStyle::Curved);
    }

    // Valid JSON that is not an object is not a canvas: loading fails and the file is not
    // treated as an empty canvas (which the next save would write over it).
    void loadRejectsNonObjectJson()
    {
        QTemporaryDir dir;
        const QString path = dir.filePath("list.canvas");
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("[1, 2, 3]");
        file.close();

        Document doc;
        doc.apply(Change{std::nullopt, card("kept", {0, 0, 10, 10})});
        QString error;
        QVERIFY(!doc.load(path, &error));
        QVERIFY(!error.isEmpty());
        QVERIFY(doc.node("kept"));
    }

    void missingAndDuplicateIdsAreReplaced()
    {
        const QJsonObject a{{"id", "a"}, {"type", "text"}};
        const QJsonObject noId{{"type", "text"}};
        Document doc;
        doc.fromJson({{"nodes", QJsonArray{a, a, noId}}});
        QSet<QString> ids;
        for (const Node &n : doc.nodes())
            ids.insert(n.id);
        QCOMPARE(ids.size(), 3);
        QVERIFY(ids.contains("a"));
        QVERIFY(!ids.contains(QString()));
    }

    // A Connection to a node the file doesn't have is kept, not silently dropped on save.
    void danglingConnectionsAreWrittenBack()
    {
        const QJsonObject edge{{"id", "e"}, {"fromNode", "a"}, {"toNode", "missing"}};
        Document doc;
        doc.fromJson({{"nodes", QJsonArray{QJsonObject{{"id", "a"}, {"type", "text"}}}}, {"edges", QJsonArray{edge}}});
        const QJsonArray edges = doc.toJson()["edges"].toArray();
        QCOMPARE(edges.size(), 1);
        QCOMPARE(edges[0]["toNode"].toString(), QString("missing"));
    }
};

QTEST_GUILESS_MAIN(TestDocument)
#include "tst_document.moc"
