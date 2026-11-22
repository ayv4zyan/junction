#pragma once

// The session of the Canvas being edited: its Document, the file it is saved to, and its undo
// history.
//
// Every change is a Transaction pushed here; it goes on the undo stack and the file is saved
// shortly afterwards. Each file keeps its undo history while the app runs, so switching to
// another Canvas and back loses nothing, unless the file was changed outside the app meanwhile.
// A file that can't be read is shown as an empty Canvas with a load error, and is never
// written to, so it stays as it was.

#include "Document.h"

#include <QDateTime>
#include <QHash>
#include <QObject>
#include <QTimer>
#include <QUndoGroup>
#include <optional>

class QUndoStack;

// QML reads these properties directly (main.cpp hands it over as `canvasSession`); changes go
// through the Editor.
class CanvasSession : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString filePath READ filePath NOTIFY filePathChanged)
    Q_PROPERTY(bool editable READ isEditable NOTIFY editableChanged)
    Q_PROPERTY(QString notEditableReason READ notEditableReason NOTIFY editableChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)

public:
    explicit CanvasSession(QObject *parent = nullptr);
    ~CanvasSession() override;   // saves what is still waiting to be saved

    const Document &document() const { return m_document; }
    QString filePath() const { return m_filePath; }
    // Only an editable Canvas takes changes and is saved. It isn't before the first open(),
    // after openNothing(), when its file can't be read (then it is never written to, so the file
    // stays as it was), or once its file was deleted.
    bool isEditable() const { return !m_notEditableBecause.has_value(); }
    // Why it isn't editable, for the user; "" while it is.
    QString notEditableReason() const { return m_notEditableBecause.value_or(QString()); }
    bool canUndo() const { return m_history.canUndo(); }
    bool canRedo() const { return m_history.canRedo(); }

    // Saves the Canvas open so far, then shows the one in `path`.
    void open(const QString &path);
    // Shows no file at all: an empty Canvas that can't be edited, for `reason`.
    void openNothing(const QString &reason);

    // Puts the Transaction on the undo stack (changes that change nothing are left out) and
    // schedules a save. Returns false when nothing went on it: it changed nothing, or the
    // Canvas can't be edited (a load error, or nothing opened yet).
    bool push(const Transaction &transaction, const QString &text);
    void undo();
    void redo();

    // Shows the items as they are without an undo step, for the drag in progress. Its final
    // state must then be pushed as a Transaction from where the drag began.
    void showUncommitted(const QVector<CanvasItem> &items);

    void save();          // now
    void savePending();   // now, if a change is waiting to be saved

    // The Library renamed, moved or deleted a file: the history and the open file follow. A
    // deleted open file can't be edited any more (it gets a load error).
    void fileMoved(const QString &oldPath, const QString &newPath);
    void fileRemoved(const QString &path);

signals:
    void opened();   // another file is shown (or none); the Document was replaced
    void filePathChanged();
    void editableChanged();   // whether it is editable, or why not
    void historyChanged();
    void errorOccurred(const QString &message);

private:
    void setFilePath(const QString &path);
    void setNotEditableBecause(const std::optional<QString> &reason);
    void activateHistory(const QString &path);

    Document m_document;
    QString m_filePath;
    std::optional<QString> m_notEditableBecause = tr("Nothing is open yet.");   // none while editable
    QUndoGroup m_history;                      // forwards undo/redo to the open file's stack
    QHash<QString, QUndoStack *> m_stacks;     // by file path
    QHash<QString, QDateTime> m_lastWritten;   // when each file was last loaded or saved by us
    QTimer m_saveTimer;
};
