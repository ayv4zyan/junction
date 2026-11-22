#include "CanvasSession.h"

#include "TransactionCommand.h"

#include <QFileInfo>
#include <QUndoStack>

CanvasSession::CanvasSession(QObject *parent)
    : QObject(parent)
{
    connect(&m_history, &QUndoGroup::canUndoChanged, this, &CanvasSession::historyChanged);
    connect(&m_history, &QUndoGroup::canRedoChanged, this, &CanvasSession::historyChanged);
    m_saveTimer.setSingleShot(true);
    m_saveTimer.setInterval(500);
    connect(&m_saveTimer, &QTimer::timeout, this, &CanvasSession::save);
}

CanvasSession::~CanvasSession()
{
    savePending();
}

void CanvasSession::open(const QString &path)
{
    savePending();
    QString error;
    if (!m_document.load(path, &error)) {
        m_document.clear();
        setNotEditableBecause(tr("%1 can't be read: %2").arg(QFileInfo(path).fileName(), error));
    } else {
        setNotEditableBecause(std::nullopt);
    }
    setFilePath(path);
    activateHistory(path);
    emit opened();
}

void CanvasSession::openNothing(const QString &reason)
{
    savePending();
    m_document.clear();
    setNotEditableBecause(reason);
    setFilePath({});
    m_history.setActiveStack(nullptr);   // no file, no history
    emit opened();
}

// The file's undo history from earlier in this session, unless the file changed since then.
void CanvasSession::activateHistory(const QString &path)
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

bool CanvasSession::push(const Transaction &transaction, const QString &text)
{
    Transaction real;
    for (const Change &change : transaction) {
        if (!change.changesNothing())
            real.append(change);
    }
    if (real.isEmpty() || !isEditable())
        return false;
    m_history.activeStack()->push(new TransactionCommand(&m_document, real, text));
    m_saveTimer.start();
    return true;
}

void CanvasSession::undo()
{
    if (!m_history.canUndo())
        return;
    m_history.undo();
    m_saveTimer.start();
}

void CanvasSession::redo()
{
    if (!m_history.canRedo())
        return;
    m_history.redo();
    m_saveTimer.start();
}

void CanvasSession::showUncommitted(const QVector<CanvasItem> &items)
{
    for (const CanvasItem &item : items)
        m_document.putUncommitted(item);
}

void CanvasSession::save()
{
    m_saveTimer.stop();
    if (!isEditable())
        return;
    QString error;
    if (!m_document.save(m_filePath, &error)) {
        emit errorOccurred(tr("Could not save %1: %2").arg(m_filePath, error));
        return;
    }
    m_lastWritten[m_filePath] = QFileInfo(m_filePath).lastModified();
}

void CanvasSession::savePending()
{
    if (m_saveTimer.isActive())
        save();
}

void CanvasSession::fileMoved(const QString &oldPath, const QString &newPath)
{
    if (QUndoStack *stack = m_stacks.take(oldPath))
        m_stacks[newPath] = stack;
    if (m_lastWritten.contains(oldPath))
        m_lastWritten[newPath] = m_lastWritten.take(oldPath);
    if (m_filePath == oldPath)
        setFilePath(newPath);
}

void CanvasSession::fileRemoved(const QString &path)
{
    delete m_stacks.take(path);   // also leaves m_history
    m_lastWritten.remove(path);
    if (m_filePath == path) {
        // Nothing may be written there any more, so it can't be edited either until the
        // Library opens another Canvas. A new Canvas may later get the same path (delete the
        // only Project, and a new "Untitled" appears): it must be loaded afresh.
        m_saveTimer.stop();
        setNotEditableBecause(tr("%1 was deleted.").arg(QFileInfo(path).fileName()));
        setFilePath({});
    }
}

void CanvasSession::setNotEditableBecause(const std::optional<QString> &reason)
{
    if (reason == m_notEditableBecause)
        return;
    m_notEditableBecause = reason;
    emit editableChanged();
}

void CanvasSession::setFilePath(const QString &path)
{
    if (path == m_filePath)
        return;
    m_filePath = path;
    emit filePathChanged();
}
