#include "OpenCanvas.h"

#include <QFile>
#include <QJsonArray>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

// The open Canvas: loading, saving, and one undo history per file.
class TestOpenCanvas : public QObject
{
    Q_OBJECT

    QTemporaryDir m_dir;

    QString writeFile(const QString &name, const QByteArray &content)
    {
        const QString path = m_dir.filePath(name);
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly))
            qFatal("can't write %s", qPrintable(path));
        file.write(content);
        return path;
    }

    static Transaction addCard(const QString &id)
    {
        Node n;
        n.id = id;
        n.rect = {0, 0, 100, 50};
        return {Change::added(n)};
    }

private slots:
    void unreadableFileIsNeverWritten()
    {
        const QString path = writeFile("broken.canvas", "{ not json");
        OpenCanvas canvas;
        canvas.open(path);
        QVERIFY(!canvas.loadError().isEmpty());
        canvas.push(addCard("a"), "Add");
        QVERIFY(canvas.document().nodes().isEmpty());
        canvas.save();
        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("{ not json"));
    }

    void savesWhatWasPushed()
    {
        const QString path = writeFile("a.canvas", "{}");
        {
            OpenCanvas canvas;
            canvas.open(path);
            canvas.push(addCard("a"), "Add");
        }   // the pending save happens on destruction
        Document reloaded;
        QString error;
        QVERIFY(reloaded.load(path, &error));
        QVERIFY(reloaded.node("a"));
    }

    void eachFileKeepsItsHistory()
    {
        const QString a = writeFile("a.canvas", "{}");
        const QString b = writeFile("b.canvas", "{}");
        OpenCanvas canvas;
        canvas.open(a);
        canvas.push(addCard("x"), "Add");
        canvas.open(b);
        QVERIFY(!canvas.canUndo());
        canvas.open(a);
        QVERIFY(canvas.canUndo());
        canvas.undo();
        QVERIFY(canvas.document().nodes().isEmpty());
    }

    void historyFollowsARenamedFile()
    {
        const QString a = writeFile("a.canvas", "{}");
        OpenCanvas canvas;
        canvas.open(a);
        canvas.push(addCard("x"), "Add");
        canvas.save();
        const QString renamed = m_dir.filePath("renamed.canvas");
        QVERIFY(QFile::rename(a, renamed));
        canvas.fileMoved(a, renamed);
        QCOMPARE(canvas.filePath(), renamed);
        QVERIFY(canvas.canUndo());
    }

    void removedFileIsNotWrittenAgain()
    {
        const QString a = writeFile("a.canvas", "{}");
        OpenCanvas canvas;
        canvas.open(a);
        canvas.push(addCard("x"), "Add");
        QVERIFY(QFile::remove(a));
        canvas.fileRemoved(a);
        canvas.save();
        QVERIFY(!QFile::exists(a));
    }
};

QTEST_GUILESS_MAIN(TestOpenCanvas)
#include "tst_open_canvas.moc"
