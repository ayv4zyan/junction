#include "Editor.h"

#include <QClipboard>
#include <QDir>
#include <QGuiApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>
#include <QTest>

// The Editor commands behind the right-click menus. Each test gets an empty Library in
// Qt's test folder, so the user's real Library is never touched.
class TestEditor : public QObject
{
    Q_OBJECT

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
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void init()
    {
        QDir(Editor::libraryFolder()).removeRecursively();
        m_editor = new Editor;
        m_editor->start();
        QVERIFY(m_editor->loadError().isEmpty());
    }

    void cleanup()
    {
        delete m_editor;
        QDir(Editor::libraryFolder()).removeRecursively();
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
        m_editor->setLabels("Backend");

        QCOMPARE(m_editor->document().connection(ids[2])->label, QString("Backend"));
        QCOMPARE(m_editor->document().node(ids[3])->label, QString("Backend"));
        QCOMPARE(m_editor->document().node(ids[0])->text, QString());
        QCOMPARE(m_editor->selectionInfo()["sharedLabel"].toString(), QString("Backend"));

        m_editor->undo();   // one step for all of them
        QCOMPARE(m_editor->document().connection(ids[2])->label, QString());
        QCOMPARE(m_editor->document().node(ids[3])->label, QString("Group"));
    }

    void removeConnectionLabels()
    {
        const QStringList ids = makeGroupedPair();
        m_editor->select(ids[2]);
        m_editor->select(ids[3], true);
        m_editor->setLabels("calls");
        m_editor->removeConnectionLabels();

        QCOMPARE(m_editor->document().connection(ids[2])->label, QString());
        QCOMPARE(m_editor->document().node(ids[3])->label, QString("calls"));   // Groups keep theirs
        QCOMPARE(m_editor->selectionInfo()["labeledConnections"].toInt(), 0);
    }

    void selectionInfoCounts()
    {
        const QStringList ids = makeGroupedPair();
        m_editor->select(ids[0]);
        m_editor->select(ids[2], true);
        m_editor->select(ids[3], true);
        const QVariantMap info = m_editor->selectionInfo();
        QCOMPARE(info["cards"].toInt(), 1);
        QCOMPARE(info["editableCards"].toInt(), 1);
        QCOMPARE(info["groups"].toInt(), 1);
        QCOMPARE(info["connections"].toInt(), 1);
        QCOMPARE(info["labelTargets"].toStringList(), (QStringList{ids[2], ids[3]}));
        QCOMPARE(info["sharedLabel"].toString(), QString());   // "" and "Group" differ
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
        m_editor->setLabels("  calls  ");
        QCOMPARE(m_editor->document().connection(ids[2])->label, QString("calls"));
        QCOMPARE(m_editor->document().node(ids[3])->label, QString("calls"));
        m_editor->setLabels("   ");
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

        m_editor->setConnectionLabel(ids[2], "verifies");
        m_editor->select(ids[2]);
        QVERIFY(m_editor->selectionBounds().top() < -5);   // the line runs at y = 0
    }
};

QTEST_MAIN(TestEditor)
#include "tst_editor.moc"
