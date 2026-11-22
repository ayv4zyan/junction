#pragma once

// The Library's folder on disk: a folder per Project, a ".canvas" file per Canvas, and the
// index file. Only the file work; the Library decides what to do and tells the user what went
// wrong, so these just say whether they worked.

#include "LibraryIndex.h"

#include <QStringList>

class LibraryFolder
{
public:
    explicit LibraryFolder(const QString &root);

    QString root() const { return m_root; }
    QString projectPath(const QString &project) const;
    QString canvasPath(const QString &project, const QString &canvas) const;   // "" for empty names

    bool create() const;                      // the folder itself, if it isn't there yet
    QStringList projectFolders() const;       // sorted by name
    QStringList canvasFiles(const QString &project) const;   // their names, without ".canvas"

    // The index: empty the first time. One that can't be read is copied to ".junction.broken"
    // (`keptAs` says where, "" if even that failed) and read as empty.
    struct IndexRead
    {
        LibraryIndex index;
        bool unreadable = false;
        QString keptAs;
    };
    IndexRead readIndex() const;
    bool writeIndex(const LibraryIndex &index) const;

    bool createProject(const QString &project) const;
    bool createEmptyCanvas(const QString &project, const QString &canvas, QString *error) const;
    bool copyFile(const QString &from, const QString &to) const;   // never replaces a file
    bool copyProject(const QString &from, const QString &to, const QStringList &canvases) const;
    // Never replaces an existing file. A change of case only ("auth" -> "Auth") also works where
    // the file system ignores case, through a temporary name. If that can't be put back,
    // `stranded` says where it ended up.
    bool move(const QString &from, const QString &to, QString *stranded) const;
    bool trash(const QString &path) const;   // a path that isn't there counts as trashed
    void removeProject(const QString &project) const;   // only for undoing a failed create

private:
    QString m_root;
};
