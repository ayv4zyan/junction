#include "OpenCanvas.h"

#include "TransactionCommand.h"

#include <QFileInfo>
#include <QUndoStack>

OpenCanvas::OpenCanvas(QObject *parent)
    : QObject(parent)
{
    connect(&m_history, &QUndoGroup::canUndoChanged, this, &OpenCanvas::historyChanged);
    connect(&m_history, &QUndoGroup::canRedoChanged, this, &OpenCanvas::historyChanged);
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(500);
    connect(&m_saveTimer, &QTimer::timeout, this, &OpenCanvas::save);
}

OpenCanvas::~OpenCanvas()
{
    savePending();
}

void OpenCanvas::open(const QString &path)
{
    savePending();
    QString error;
    if (!m_document.load(path, &error)) {
        m_document.clear();
        m_loadError = tr("%1 can't be read: %2").arg(QFileInfo(path).fileName(), error);
    } else {
        m_loadError.clear();
    }
    setFilePath(path);
    activateHistory(path);
    emit opened();
}

void OpenCanvas::openNothing(const QString &reason)
{
    savePending();
    m_document.clear();
    m_loadError = reason;
    setFilePath({});
    activateHistory({});
    emit opened();
}

// The file's undo history from earlier in this session, unless the file changed since then.
void OpenCanvas::activateHistory(const QString &path)
{
    QUndoStack *&stack = m_stacks[path];
    const QDateTime modified = QFileInfo(path).lastModified();
    if (!stack) {
        stack = new QUndoStack(&m_history);
        m_history.addStack(stack);
    } else if (m_lastWritten.value(path) != modified) {
        stack->clear();
    }
    m_lastWritten[path] = modified;
    m_history.setActiveStack(stack);
}

void OpenCanvas::push(const Transaction &transaction, const QString &text)
{
    Transaction real;
    for (const Change &change : transaction) {
        if (change.before != change.after)
            real.append(change);
    }
    if (real.isEmpty() || !m_loadError.isEmpty() || !m_history.activeStack())   // nothing opened yet
        return;
    m_history.activeStack()->push(new TransactionCommand(&m_document, real, text));
    m_saveTimer.start();
}

void OpenCanvas::undo()
{
    if (!m_history.canUndo())
        return;
    m_history.undo();
    m_saveTimer.start();
}

void OpenCanvas::redo()
{
    if (!m_history.canRedo())
        return;
    m_history.redo();
    m_saveTimer.start();
}

void OpenCanvas::showUncommitted(const QVector<Item> &items)
{
    for (const Item &item : items)
        m_document.apply(Change{std::nullopt, item});
}

void OpenCanvas::save()
{
    m_saveTimer.stop();
    if (m_filePath.isEmpty() || !m_loadError.isEmpty())
        return;
    QString error;
    if (!m_document.save(m_filePath, &error)) {
        emit errorOccurred(tr("Could not save %1: %2").arg(m_filePath, error));
        return;
    }
    m_lastWritten[m_filePath] = QFileInfo(m_filePath).lastModified();
}

void OpenCanvas::savePending()
{
    if (m_saveTimer.isActive())
        save();
}

void OpenCanvas::fileMoved(const QString &oldPath, const QString &newPath)
{
    if (QUndoStack *stack = m_stacks.take(oldPath))
        m_stacks[newPath] = stack;
    if (m_lastWritten.contains(oldPath))
        m_lastWritten[newPath] = m_lastWritten.take(oldPath);
    if (m_filePath == oldPath)
        setFilePath(newPath);
}

void OpenCanvas::fileRemoved(const QString &path)
{
    delete m_stacks.take(path);   // also leaves m_history
    m_lastWritten.remove(path);
    if (m_filePath == path) {
        // Nothing may be written there any more, so it can't be edited either until the
        // Library opens another Canvas. A new Canvas may later get the same path (delete the
        // only Project, and a new "Untitled" appears): it must be loaded afresh.
        m_saveTimer.stop();
        m_loadError = tr("%1 was deleted.").arg(QFileInfo(path).fileName());
        setFilePath({});
    }
}

void OpenCanvas::setFilePath(const QString &path)
{
    if (path == m_filePath)
        return;
    m_filePath = path;
    emit filePathChanged();
}
