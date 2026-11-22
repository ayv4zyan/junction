#include "Editor.h"

#include <QClipboard>
#include <QFileInfo>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

// The Editor commands behind the right-click menus. Each test gets an empty Library in a
// temporary folder, so the user's real Library is never touched.
class TestEditor : public QObject
{
    Q_OBJECT

    std::unique_ptr<QTemporaryDir> m_folder;
    std::unique_ptr<Library> m_library;
    Editor *m_editor = nullptr;

    // Two Cards connected A -> B, with a Group around both. Returns {a, b, connection, group}.
    QStringList makeGroupedPair()
    {
        Editor &e = *m_editor;
        const QString a = e.createCard({0, 0});
        const QString b = e.createCard({400, 0});
        e.connectNodes(a, b, {});
        const QString connection = e.selection().first();
        e.select(a);
        e.select(b, true);
        const QString group = e.createGroup({});
        return {a, b, connection, group};
    }

    int clipboardNodeCount() const
    {
        const QJsonObject json = QJsonDocument::fromJson(QGuiApplication::clipboard()->text().toUtf8()).object();
        return json["nodes"].toArray().size();
    }

private slots:
    void init()
    {
        m_folder = std::make_unique<QTemporaryDir>();
        m_library = std::make_unique<Library>(m_folder->filePath("Library"));
        m_editor = new Editor(m_library.get(), QGuiApplication::clipboard());
        m_library->load();
        QVERIFY(m_editor->openCanvas()->notEditableReason().isEmpty());
    }

    void cleanup()
    {
        delete m_editor;
        m_library.reset();
        m_folder.reset();
    }

    void cutGroupTakesItsCards()
    {
        const QStringList ids = makeGroupedPair();
        m_editor->select(ids[3]);   // only the Group
        m_editor->cut();

        QCOMPARE(clipboardNodeCount(), 3);   // the Group and both Cards
        QVERIFY(m_editor->document().nodes().isEmpty());
        QVERIFY(m_editor->document().connections().isEmpty());

        m_editor->undo();   // one step brings everything back
        QCOMPARE(m_editor->document().nodes().size(), 3);
        QCOMPARE(m_editor->document().connections().size(), 1);
    }

    void deleteGroupKeepsItsCards()
    {
        const QStringList ids = makeGroupedPair();
        m_editor->select(ids[3]);
        m_editor->deleteSelection();
        QCOMPARE(m_editor->document().nodes().size(), 2);
    }

    void setLabelsOnGroupsAndConnections()
    {
        const QStringList ids = makeGroupedPair();
        m_editor->select(ids[0]);              // a Card: has no Label, stays as it is
        m_editor->select(ids[2], true);        // the Connection
        m_editor->select(ids[3], true);        // the Group
        m_editor->setLabels(m_editor->selection(), "Backend");

        QCOMPARE(m_editor->document().connection(ids[2])->label, QString("Backend"));
        QCOMPARE(m_editor->document().node(ids[3])->label(), QString("Backend"));
        QCOMPARE(m_editor->document().node(ids[0])->text(), QString());
        QCOMPARE(m_editor->selectionSummary().sharedLabel, QString("Backend"));

        m_editor->undo();   // one step for all of them
        QCOMPARE(m_editor->document().connection(ids[2])->label, QString());
        QCOMPARE(m_editor->document().node(ids[3])->label(), QString("Group"));
    }

    void removeConnectionLabels()
    {
        const QStringList ids = makeGroupedPair();
        m_editor->select(ids[2]);
        m_editor->select(ids[3], true);
        m_editor->setLabels(m_editor->selection(), "calls");
        m_editor->removeConnectionLabels();

        QCOMPARE(m_editor->document().connection(ids[2])->label, QString());
        QCOMPARE(m_editor->document().node(ids[3])->label(), QString("calls"));   // Groups keep theirs
        QCOMPARE(m_editor->selectionSummary().labeledConnections, 0);
    }

    void selectionSummaryCounts()
    {
        const QStringList ids = makeGroupedPair();
        m_editor->select(ids[0]);
        m_editor->select(ids[2], true);
        m_editor->select(ids[3], true);
        const SelectionSummary summary = m_editor->selectionSummary();
        QCOMPARE(summary.cards, 1);
        QCOMPARE(summary.editableCards, 1);
        QCOMPARE(summary.groups, 1);
        QCOMPARE(summary.connections, 1);
        QCOMPARE(summary.labelTargets, (QStringList{ids[2], ids[3]}));
        QCOMPARE(summary.sharedLabel, QString());   // "" and "Group" differ
    }

    // What the floating selection toolbar is placed against, in world coordinates.
    // Cards are 240x80 around where they were created.
    void selectionBoundsCoverTheSelectedItems()
    {
        const QStringList ids = makeGroupedPair();
        m_editor->clearSelection();
        QVERIFY(m_editor->selectionBounds().isEmpty());

        m_editor->select(ids[0]);
        m_editor->select(ids[1], true);
        QCOMPARE(m_editor->selectionBounds(), QRectF(-120, -40, 640, 80));

        // a Connection alone: its line, from A's right side to B's left side
        m_editor->select(ids[2]);
        const QRectF line = m_editor->selectionBounds();
        QCOMPARE(line.left(), 120.0);
        QCOMPARE(line.right(), 280.0);
    }

    // Cut removes the selected Connections too, even without their Cards.
    void cutRemovesSelectedConnections()
    {
        const QStringList ids = makeGroupedPair();
        m_editor->select(ids[2]);   // only the Connection
        m_editor->cut();
        QVERIFY(!m_editor->document().connection(ids[2]));
        QVERIFY(m_editor->document().node(ids[0]));
        QVERIFY(m_editor->document().node(ids[1]));
    }

    // Like a single Connection's label: surrounding spaces go, so "   " means no label.
    void setLabelsTrimsSpaces()
    {
        const QStringList ids = makeGroupedPair();
        m_editor->select(ids[2]);
        m_editor->select(ids[3], true);
        m_editor->setLabels(m_editor->selection(), "  calls  ");
        QCOMPARE(m_editor->document().connection(ids[2])->label, QString("calls"));
        QCOMPARE(m_editor->document().node(ids[3])->label(), QString("calls"));
        m_editor->setLabels(m_editor->selection(), "   ");
        QCOMPARE(m_editor->document().connection(ids[2])->label, QString());
    }

    // The toolbar must not cover what belongs to the selection: a Group's label bar sits above
    // the Group (GroupItem: a 28px bar, 4px up), a Connection's label sticks out of its line.
    void selectionBoundsIncludeLabels()
    {
        const QStringList ids = makeGroupedPair();
        const QRectF group = m_editor->document().node(ids[3])->rect;
        m_editor->select(ids[3]);
        QCOMPARE(m_editor->selectionBounds().top(), group.top() - 32);

        m_editor->setLabels({ids[2]}, "verifies");
        m_editor->select(ids[2]);
        QVERIFY(m_editor->selectionBounds().top() < -5);   // the line runs at y = 0
    }

    // A drag shows every step on the Canvas but is one undo step, and snaps to the grid.
    void movingIsOneUndoStep()
    {
        const QString a = m_editor->createCard({0, 0});
        const QPointF start = m_editor->document().node(a)->rect.topLeft();
        m_editor->beginMove(a);
        QVERIFY(m_editor->isDragging());
        m_editor->updateMove({7, 3}, false);
        m_editor->updateMove({43, 18}, false);
        QCOMPARE(m_editor->document().node(a)->rect.topLeft(), start + QPointF(40, 20));
        m_editor->endMove();
        QVERIFY(!m_editor->isDragging());
        m_editor->undo();
        QCOMPARE(m_editor->document().node(a)->rect.topLeft(), start);
        QVERIFY(m_editor->document().node(a));   // undoing the move did not undo the creation
    }

    // Nothing else changes the Canvas during a drag: its final Transaction starts from where it began.
    void commandsWaitForTheDragToEnd()
    {
        const QString a = m_editor->createCard({0, 0});
        m_editor->beginMove(a);
        m_editor->deleteSelection();
        QVERIFY(m_editor->document().node(a));
        m_editor->endMove();
    }

    // Opening another Canvas mid-drag drops the drag: its release must not commit the old
    // Canvas's items into the new one.
    void openingAnotherCanvasCancelsTheDrag()
    {
        const QString a = m_editor->createCard({0, 0});
        const QString first = m_library->currentPath();
        m_editor->beginMove(a);
        m_editor->updateMove({200, 0}, false);
        m_library->createCanvas(m_library->currentProject());
        QVERIFY(!m_editor->isDragging());
        m_editor->endMove();
        QVERIFY(m_editor->document().nodes().isEmpty());
        QVERIFY(!m_editor->openCanvas()->canUndo());

        m_library->open(m_library->currentProject(), QFileInfo(first).completeBaseName());
        QCOMPARE(m_editor->document().node(a)->rect.left(), -120.0);   // where it was before the drag
    }

    // A command that can't be done (here: during a drag) neither selects nor returns an item
    // that was never added.
    void creatingDuringADragAddsAndSelectsNothing()
    {
        const QString a = m_editor->createCard({0, 0});
        m_editor->beginMove(a);
        QCOMPARE(m_editor->createCard({400, 0}), QString());
        QCOMPARE(m_editor->selection(), QStringList{a});
        m_editor->endMove();
        QCOMPARE(m_editor->document().nodes().size(), 1);
    }

    // A node whose kind changes under the same id moves between the Cards and the Groups.
    void aNodeThatChangesKindMovesToTheOtherModel()
    {
        Document doc;
        NodeModel cards(&doc, NodeModel::Cards);
        NodeModel groups(&doc, NodeModel::Groups);
        Node n;
        n.id = "x";
        n.rect = {0, 0, 100, 50};
        doc.put(n);
        QCOMPARE(cards.rowCount(), 1);
        n.content = GroupContent{"G"};
        doc.put(n);
        QCOMPARE(cards.rowCount(), 0);
        QCOMPARE(groups.rowCount(), 1);
    }

    void resizingKeepsAMinimumSize()
    {
        const QString a = m_editor->createCard({0, 0});
        const QRectF before = m_editor->document().node(a)->rect;
        m_editor->beginResize(a, Qt::RightEdge);
        m_editor->updateResize({-1000, 0}, false);
        m_editor->endResize();
        const QRectF after = m_editor->document().node(a)->rect;
        QCOMPARE(after.left(), before.left());
        QCOMPARE(after.width(), 80.0);
    }

    void draggingAMidpointAddsAWaypoint()
    {
        const QStringList ids = makeGroupedPair();
        m_editor->beginWaypointDrag(ids[2], 0, true);
        QCOMPARE(m_editor->document().connection(ids[2])->waypoints.size(), 1);
        m_editor->updateWaypointDrag({0, 100}, false);
        m_editor->endWaypointDrag();
        const Connection *c = m_editor->document().connection(ids[2]);
        QCOMPARE(c->waypoints.size(), 1);
        QCOMPARE(int(c->waypoints.first().y()) % 20, 0);   // snapped
        m_editor->undo();
        QVERIFY(m_editor->document().connection(ids[2])->waypoints.isEmpty());
    }
};

QTEST_MAIN(TestEditor)
#include "tst_editor.moc"
