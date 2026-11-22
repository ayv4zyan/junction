#include "Fragment.h"

#include <QJsonArray>
#include <QTest>

// Copying a piece of a Canvas and inserting it again (copy, paste, duplicate).
class TestFragment : public QObject
{
    Q_OBJECT

    static Node card(const QString &id, QRectF rect)
    {
        Node n;
        n.id = id;
        n.rect = rect;
        return n;
    }

    static Connection connection(const QString &id, const QString &from, const QString &to)
    {
        Connection c;
        c.id = id;
        c.fromNode = from;
        c.toNode = to;
        c.waypoints = {{50, 50}};
        return c;
    }

    // a -> b -> c
    static void fill(Document &doc)
    {
        doc.apply(Transaction{Change::added(card("a", {0, 0, 100, 50})),
                              Change::added(card("b", {200, 0, 100, 50})),
                              Change::added(card("c", {400, 0, 100, 50})),
                              Change::added(connection("ab", "a", "b")),
                              Change::added(connection("bc", "b", "c"))});
    }

private slots:
    void copyTakesConnectionsBetweenCopiedNodesOnly()
    {
        Document doc;
        fill(doc);
        const QJsonObject fragment = Fragment::copy(doc, {"a", "b"});
        QCOMPARE(fragment["nodes"].toArray().size(), 2);
        QCOMPARE(fragment["edges"].toArray().size(), 1);
        QCOMPARE(Fragment::bounds(fragment), QRectF(0, 0, 300, 50));
    }

    void insertionHasFreshIdsAndIsMoved()
    {
        Document doc;
        fill(doc);
        const Fragment::Insertion insertion = Fragment::insertion(Fragment::copy(doc, {"a", "b"}), {10, 20});
        QCOMPARE(insertion.nodeIds.size(), 2);
        QCOMPARE(insertion.transaction.size(), 3);   // two nodes, one Connection
        doc.apply(insertion.transaction);
        QCOMPARE(doc.nodes().size(), 5);
        const Node *copy = doc.node(insertion.nodeIds.first());
        QVERIFY(copy && copy->id != "a");
        QCOMPARE(copy->rect.topLeft(), QPointF(10, 20));
        const Connection &copied = doc.connections().last();
        QVERIFY(insertion.nodeIds.contains(copied.fromNode) && insertion.nodeIds.contains(copied.toNode));
        QCOMPARE(copied.waypoints.first(), QPointF(60, 70));
    }

    void textIsNotAFragment()
    {
        QVERIFY(Fragment::bounds({}).isEmpty());
        QVERIFY(Fragment::insertion({}, {}).transaction.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestFragment)
#include "tst_fragment.moc"
