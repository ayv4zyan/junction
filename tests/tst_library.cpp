#include "Library.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

class TestLibrary : public QObject
{
    Q_OBJECT

    // Next to the test binary (not in /tmp): deleting moves files to the trash, and the trash
    // used is the one on the same disk as the home folder.
    QTemporaryDir m_temp{QCoreApplication::applicationDirPath() + "/library-test-XXXXXX"};
    QString m_root;

    static QStringList canvases(const Library &library, int project)
    {
        QStringList names;
        for (const CanvasEntry &c : library.index().projects[project].canvases)
            names.append(c.name);
        return names;
    }

    static QStringList projects(const Library &library)
    {
        QStringList names;
        for (const ProjectEntry &p : library.index().projects)
            names.append(p.name);
        return names;
    }

    void touch(const QString &relativePath)
    {
        const QString path = QDir(m_root).filePath(relativePath);
        QDir().mkpath(QFileInfo(path).absolutePath());
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{}");
    }

private slots:
    void initTestCase()
    {
        // Keep that trash inside the temporary folder too.
        QDir().mkpath(m_temp.filePath("data"));   // Qt only uses a trash whose parent exists
        qputenv("XDG_DATA_HOME", m_temp.filePath("data").toUtf8());
    }

    void init()
    {
        m_root = m_temp.filePath(QStringLiteral("library-%1").arg(QTest::currentTestFunction()));
        QDir(m_root).removeRecursively();
    }

    void firstLoadCreatesAnUntitledProject()
    {
        Library library(m_root);
        library.load();
        QCOMPARE(projects(library), QStringList{"Untitled"});
        QCOMPARE(canvases(library, 0), QStringList{"Untitled"});
        QCOMPARE(library.currentPath(), QDir(m_root).filePath("Untitled/Untitled.canvas"));
        QVERIFY(QFile::exists(library.currentPath()));
    }

    // An index that can't be read is reported and kept aside, not silently replaced.
    void brokenIndexIsKeptAndReported()
    {
        touch("Notes/Ideas.canvas");
        QFile index(QDir(m_root).filePath(".junction"));
        QVERIFY(index.open(QIODevice::WriteOnly));
        index.write("{ broken");
        index.close();

        Library library(m_root);
        QSignalSpy errors(&library, &Library::errorOccurred);
        library.load();
        QCOMPARE(errors.count(), 1);
        QCOMPARE(projects(library), QStringList{"Notes"});
        QFile kept(QDir(m_root).filePath(".junction.broken"));
        QVERIFY(kept.open(QIODevice::ReadOnly));
        QCOMPARE(kept.readAll(), QByteArray("{ broken"));
    }

    void indexKeepsOrderViewsAndCurrent()
    {
        {
            Library library(m_root);
            library.load();
            library.renameProject("Untitled", "Web");
            library.createCanvas("Web");          // "Untitled"
            library.renameCanvas("Web", "Untitled", "Zeta");
            library.createCanvas("Web");          // "Untitled" again
            library.moveCanvas("Web", "Untitled", "Web", 0);
            library.createProject();
            library.moveProject("Untitled", 0);
            library.setView("Web", "Zeta", {QPointF(10, 20), 2});
            library.setExpanded("Web", false);
            library.open("Web", "Zeta");
        }
        Library library(m_root);
        library.load();
        QCOMPARE(projects(library), (QStringList{"Untitled", "Web"}));
        QCOMPARE(canvases(library, 1), (QStringList{"Untitled", "Zeta", "Untitled 2"}));
        QCOMPARE(library.currentProject(), "Web");
        QCOMPARE(library.currentCanvas(), "Zeta");
        QVERIFY(library.index().projects[1].expanded);   // opening a Canvas shows it in the sidebar
        const auto view = library.view("Web", "Zeta");
        QVERIFY(view.has_value());
        QCOMPARE(view->center, QPointF(10, 20));
        QCOMPARE(view->zoom, 2.0);
    }

    void loadPicksUpChangesMadeOutsideTheApp()
    {
        {
            Library library(m_root);
            library.load();
            library.renameProject("Untitled", "B");
            library.renameCanvas("B", "Untitled", "Two");
            library.createCanvas("B");
            library.renameCanvas("B", "Untitled", "One");
        }
        // B/Two.canvas disappears, B/Three.canvas and the folders A and Empty appear.
        QFile::remove(QDir(m_root).filePath("B/Two.canvas"));
        touch("B/Three.canvas");
        touch("A/x.canvas");
        touch("A/notes.txt");
        QDir(m_root).mkpath("Empty");

        Library library(m_root);
        library.load();
        QCOMPARE(projects(library), (QStringList{"B", "A", "Empty"}));
        QCOMPARE(canvases(library, 0), (QStringList{"One", "Three"}));
        QCOMPARE(canvases(library, 1), QStringList{"x"});
        QCOMPARE(canvases(library, 2), QStringList{"Untitled"});   // a Project is never empty
        // "Two" was open; its Project's first Canvas takes its place.
        QCOMPARE(library.currentProject(), "B");
        QCOMPARE(library.currentCanvas(), "One");
    }

    void namesAreCheckedIgnoringCase()
    {
        Library library(m_root);
        library.load();
        library.createCanvas("Untitled");   // "Untitled 2"
        QCOMPARE(canvases(library, 0), (QStringList{"Untitled", "Untitled 2"}));

        QVERIFY(!library.canvasNameProblem("Untitled", "untitled 2").isEmpty());
        QVERIFY(library.canvasNameProblem("Untitled", "untitled 2", "Untitled 2").isEmpty());
        QVERIFY(!library.canvasNameProblem("Untitled", "  ").isEmpty());
        QVERIFY(!library.canvasNameProblem("Untitled", ".hidden").isEmpty());
        QVERIFY(!library.canvasNameProblem("Untitled", "a/b").isEmpty());
        QVERIFY(!library.canvasNameProblem("Untitled", "what?").isEmpty());
        QVERIFY(library.canvasNameProblem("Untitled", "  Auth  ").isEmpty());

        QSignalSpy errors(&library, &Library::errorOccurred);
        QVERIFY(!library.renameCanvas("Untitled", "Untitled", "UNTITLED 2"));
        QCOMPARE(errors.size(), 1);
        QVERIFY(library.renameCanvas("Untitled", "Untitled", "  Auth "));
        QCOMPARE(canvases(library, 0), (QStringList{"Auth", "Untitled 2"}));
        QVERIFY(QFile::exists(library.canvasPath("Untitled", "Auth")));
    }

    void renamingOnlyTheCase()
    {
        Library library(m_root);
        library.load();
        QSignalSpy moved(&library, &Library::canvasMoved);
        QVERIFY(library.renameCanvas("Untitled", "Untitled", "untitled"));
        QVERIFY(library.renameProject("Untitled", "UNTITLED"));
        QCOMPARE(projects(library), QStringList{"UNTITLED"});
        QCOMPARE(library.currentPath(), QDir(m_root).filePath("UNTITLED/untitled.canvas"));
        QVERIFY(QFile::exists(library.currentPath()));
        QCOMPARE(moved.size(), 2);
    }

    void renamingAProjectMovesItsCanvases()
    {
        Library library(m_root);
        library.load();
        library.createCanvas("Untitled");
        QSignalSpy moved(&library, &Library::canvasMoved);
        QSignalSpy current(&library, &Library::currentChanged);
        QVERIFY(library.renameProject("Untitled", "Web"));
        QCOMPARE(moved.size(), 2);
        QCOMPARE(moved[0][1].toString(), QDir(m_root).filePath("Web/Untitled.canvas"));
        QCOMPARE(current.size(), 1);
        QCOMPARE(library.currentProject(), "Web");
    }

    void theLastCanvasCantBeDeleted()
    {
        Library library(m_root);
        library.load();
        QSignalSpy errors(&library, &Library::errorOccurred);
        QVERIFY(!library.deleteCanvas("Untitled", "Untitled"));
        QCOMPARE(errors.size(), 1);
        QVERIFY(QFile::exists(library.currentPath()));
    }

    void deletingTheOpenCanvasOpensItsNeighbour()
    {
        Library library(m_root);
        library.load();
        library.createCanvas("Untitled");   // Untitled 2
        library.createCanvas("Untitled");   // Untitled 3
        library.open("Untitled", "Untitled 2");
        const QString path = library.currentPath();
        QSignalSpy removed(&library, &Library::canvasRemoved);
        QVERIFY(library.deleteCanvas("Untitled", "Untitled 2"));
        QCOMPARE(removed.size(), 1);
        QCOMPARE(removed[0][0].toString(), path);
        QVERIFY(!QFile::exists(path));
        QCOMPARE(library.currentCanvas(), "Untitled 3");   // the one that took its place
        QVERIFY(library.deleteCanvas("Untitled", "Untitled 3"));
        QCOMPARE(library.currentCanvas(), "Untitled");     // the last one: the one before
    }

    void deletingTheLastProjectLeavesANewOne()
    {
        Library library(m_root);
        library.load();
        library.renameProject("Untitled", "Web");
        library.createProject();   // Untitled
        library.open("Web", "Untitled");
        QVERIFY(library.deleteProject("Web"));
        QCOMPARE(projects(library), QStringList{"Untitled"});
        QCOMPARE(library.currentProject(), "Untitled");
        QVERIFY(!QDir(QDir(m_root).filePath("Web")).exists());

        QVERIFY(library.deleteProject("Untitled"));
        QCOMPARE(projects(library), QStringList{"Untitled"});   // a fresh, empty one
        QVERIFY(QFile::exists(library.currentPath()));
    }

    void movingACanvasToAnotherProject()
    {
        Library library(m_root);
        library.load();
        library.renameProject("Untitled", "A");
        library.createCanvas("A");   // A/Untitled 2
        library.createProject();     // Untitled/Untitled
        library.renameProject("Untitled", "B");
        library.open("A", "Untitled");
        library.setView("A", "Untitled", {QPointF(1, 2), 0.5});

        QSignalSpy moved(&library, &Library::canvasMoved);
        // The name is taken in B, so it becomes "Untitled 2"; it lands at the top.
        QCOMPARE(library.moveCanvas("A", "Untitled", "B", 0), QString("Untitled 2"));
        QCOMPARE(canvases(library, 0), QStringList{"Untitled 2"});
        QCOMPARE(canvases(library, 1), (QStringList{"Untitled 2", "Untitled"}));
        QCOMPARE(moved.size(), 1);
        QVERIFY(QFile::exists(library.canvasPath("B", "Untitled 2")));
        QCOMPARE(library.currentProject(), "B");   // the open Canvas went with it
        QCOMPARE(library.currentCanvas(), "Untitled 2");
        QCOMPARE(library.view("B", "Untitled 2")->zoom, 0.5);

        // A's only Canvas has to stay.
        QSignalSpy errors(&library, &Library::errorOccurred);
        QCOMPARE(library.moveCanvas("A", "Untitled 2", "B", 0), std::nullopt);
        QCOMPARE(errors.size(), 1);
    }

    // A Project remembers the Canvas last open in it; one that moved away is no longer it.
    void movingTheOpenCanvasUpdatesBothProjects()
    {
        Library library(m_root);
        library.load();
        library.createCanvas("Untitled");   // Untitled 2, open
        library.createProject();            // Untitled 2 (Project)
        library.open("Untitled", "Untitled 2");
        QCOMPARE(library.moveCanvas("Untitled", "Untitled 2", "Untitled 2", 0), QString("Untitled 2"));
        QCOMPARE(library.index().lastOpenedCanvas("Untitled 2"), QString("Untitled 2"));
        QCOMPARE(library.index().lastOpenedCanvas("Untitled"), QString("Untitled"));
        QCOMPARE(library.currentProject(), QString("Untitled 2"));
    }

    // A new empty Project or Canvas is announced, so the UI can have it named right away.
    void creatingSaysWhatWasCreated()
    {
        Library library(m_root);
        library.load();
        QSignalSpy created(&library, &Library::createdEmpty);
        library.createCanvas("Untitled");
        library.createProject();
        QCOMPARE(created.count(), 2);
        QCOMPARE(created[0], (QVariantList{"Untitled", "Untitled 2"}));
        QCOMPARE(created[1], (QVariantList{"Untitled 2", QString()}));
    }

    void openingWithoutACanvasOpensTheLastOneThereAndSteppingMovesAlong()
    {
        Library library(m_root);
        library.load();
        library.createCanvas("Untitled");   // Untitled 2, open
        library.createProject();
        QVERIFY(library.openProject("Untitled"));
        QCOMPARE(library.currentCanvas(), QString("Untitled 2"));
        library.openAdjacent(-1);
        QCOMPARE(library.currentCanvas(), QString("Untitled"));
        library.openAdjacent(-1);   // already the first: stays
        QCOMPARE(library.currentCanvas(), QString("Untitled"));
    }

    // The order the Editor relies on (see Library.h): a renamed open Canvas is reported moved
    // before it is reported current.
    void renamingTheOpenCanvasSaysMovedBeforeCurrent()
    {
        Library library(m_root);
        library.load();
        QStringList heard;
        connect(&library, &Library::openCanvasMustBeSaved, this, [&] { heard << "openCanvasMustBeSaved"; });
        connect(&library, &Library::canvasMoved, this, [&] { heard << "canvasMoved"; });
        connect(&library, &Library::projectsChanged, this, [&] { heard << "projectsChanged"; });
        connect(&library, &Library::currentChanged, this, [&] { heard << "currentChanged"; });
        QVERIFY(library.renameCanvas("Untitled", "Untitled", "Notes"));
        QCOMPARE(heard, (QStringList{"openCanvasMustBeSaved", "canvasMoved", "projectsChanged", "currentChanged"}));
    }

    void reorderingCountsPositionsBeforeTheMove()
    {
        Library library(m_root);
        library.load();
        library.createCanvas("Untitled");   // Untitled 2
        library.createCanvas("Untitled");   // Untitled 3
        library.moveCanvas("Untitled", "Untitled", "Untitled", 3);   // to the end
        QCOMPARE(canvases(library, 0), (QStringList{"Untitled 2", "Untitled 3", "Untitled"}));
        library.moveCanvas("Untitled", "Untitled", "Untitled", 1);   // between the first two
        QCOMPARE(canvases(library, 0), (QStringList{"Untitled 2", "Untitled", "Untitled 3"}));
    }

    void duplicating()
    {
        Library library(m_root);
        library.load();
        library.renameCanvas("Untitled", "Untitled", "Auth");
        library.createCanvas("Untitled");
        QCOMPARE(library.duplicateCanvas("Untitled", "Auth"), QString("Auth copy"));
        QCOMPARE(canvases(library, 0), (QStringList{"Auth", "Auth copy", "Untitled"}));
        QCOMPARE(library.duplicateCanvas("Untitled", "Auth"), QString("Auth copy 2"));
        QCOMPARE(library.currentCanvas(), "Auth copy 2");

        QCOMPARE(library.duplicateProject("Untitled"), QString("Untitled copy"));
        QCOMPARE(projects(library), (QStringList{"Untitled", "Untitled copy"}));
        QCOMPARE(canvases(library, 1), canvases(library, 0));
        QVERIFY(QFile::exists(library.canvasPath("Untitled copy", "Auth copy 2")));
    }

    void importing()
    {
        const QString outside = m_temp.filePath("Shared board.canvas");
        {
            QFile file(outside);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(R"({"nodes":[{"id":"a","type":"text","x":0,"y":0,"width":10,"height":10,"text":"hi"}],"edges":[]})");
        }
        const QString broken = m_temp.filePath("broken.canvas");
        {
            QFile file(broken);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write("not json");
        }

        Library library(m_root);
        library.load();
        QCOMPARE(library.importCanvas(outside, "Untitled"), QString("Shared board"));
        QCOMPARE(library.importCanvas(outside, "Untitled"), QString("Shared board 2"));
        QCOMPARE(library.currentCanvas(), "Shared board 2");
        QVERIFY(QFile::exists(outside));   // a copy, not a move

        QSignalSpy errors(&library, &Library::errorOccurred);
        QCOMPARE(library.importCanvas(broken, "Untitled"), std::nullopt);
        QCOMPARE(errors.size(), 1);
    }

    void projectsRememberTheirLastCanvas()
    {
        {
            Library library(m_root);
            library.load();
            library.createCanvas("Untitled");   // Untitled 2, opened
            library.createProject();            // the Project "Untitled 2", opened
            QCOMPARE(library.index().lastOpenedCanvas("Untitled"), "Untitled 2");
            library.renameCanvas("Untitled", "Untitled 2", "Auth");
            QCOMPARE(library.index().lastOpenedCanvas("Untitled"), "Auth");
        }
        Library library(m_root);
        library.load();
        QCOMPARE(library.index().lastOpenedCanvas("Untitled"), "Auth");
        QCOMPARE(library.index().lastOpenedCanvas("Untitled 2"), "Untitled");
    }

    void openingExpandsTheProjectWithoutChangingTheList()
    {
        Library library(m_root);
        library.load();
        library.createCanvas("Untitled");
        library.setExpanded("Untitled", false);
        QSignalSpy changed(&library, &Library::projectsChanged);
        QSignalSpy expanded(&library, &Library::expandedChanged);
        library.open("Untitled", "Untitled");
        QCOMPARE(changed.size(), 0);
        QCOMPARE(expanded.size(), 1);
        QVERIFY(library.index().projects[0].expanded);
    }

    void openingSavesTheOldCanvasFirst()
    {
        Library library(m_root);
        library.load();
        library.createCanvas("Untitled");
        QStringList order;
        connect(&library, &Library::openCanvasMustBeSaved, this, [&] { order << "aboutToChange:" + library.currentCanvas(); });
        connect(&library, &Library::currentChanged, this, [&] { order << "current:" + library.currentCanvas(); });
        library.open("Untitled", "Untitled");
        QCOMPARE(order, (QStringList{"aboutToChange:Untitled 2", "current:Untitled"}));
    }
};

QTEST_GUILESS_MAIN(TestLibrary)
#include "tst_library.moc"
