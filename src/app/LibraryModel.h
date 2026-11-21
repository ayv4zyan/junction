#pragma once

// The Library as the sidebar shows it: one flat list of rows, a Project row followed by its
// Canvas rows (while it is expanded). With a search filter, only matching rows are shown, all
// expanded. Also the place QML calls to organise the Library; it just forwards to Library.

#include "Library.h"

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QUrl>

class LibraryModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Provided by Editor")

    Q_PROPERTY(QString currentProject READ currentProject NOTIFY currentChanged)
    Q_PROPERTY(QString currentCanvas READ currentCanvas NOTIFY currentChanged)
    Q_PROPERTY(int currentRow READ currentRow NOTIFY currentChanged)
    Q_PROPERTY(QStringList projectNames READ projectNames NOTIFY structureChanged)
    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)
    Q_PROPERTY(QUrl folder READ folder CONSTANT)

public:
    enum Role {
        KindRole = Qt::UserRole + 1,   // "project" or "canvas"
        NameRole,
        ProjectRole,          // the Project's name (for Canvas rows: the Project it is in)
        ProjectIndexRole,     // position of that Project in the Library
        CanvasIndexRole,      // position of the Canvas in its Project, -1 for Project rows
        CanvasCountRole,      // how many Canvases the Project has
        ExpandedRole,
        CurrentRole,          // the open Canvas, or the Project it is in
    };

    explicit LibraryModel(Library *library, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString currentProject() const { return m_library->currentProject(); }
    QString currentCanvas() const { return m_library->currentCanvas(); }
    int currentRow() const { return rowOf(currentProject(), currentCanvas()); }
    QStringList projectNames() const;
    QString filter() const { return m_filter; }
    void setFilter(const QString &filter);
    QUrl folder() const { return QUrl::fromLocalFile(m_library->root()); }

    Q_INVOKABLE QVariantMap rowAt(int row) const;   // the roles of one row, by role name
    Q_INVOKABLE int rowOf(const QString &project, const QString &canvas = {}) const;
    Q_INVOKABLE int lastRowOf(const QString &project) const;   // its last visible Canvas row
    Q_INVOKABLE int canvasCount(const QString &project) const;

    Q_INVOKABLE void open(const QString &project, const QString &canvas = {});   // no Canvas: the last one open there
    Q_INVOKABLE void openAdjacent(int step);   // the previous (-1) or next (1) Canvas in the Project
    Q_INVOKABLE bool openFirstMatch();          // the first Canvas the search shows

    Q_INVOKABLE void createProject();
    Q_INVOKABLE void createCanvas(const QString &project);
    Q_INVOKABLE void duplicateProject(const QString &project);
    Q_INVOKABLE void duplicateCanvas(const QString &project, const QString &canvas);
    Q_INVOKABLE void importCanvases(const QList<QUrl> &urls);   // into the current Project

    Q_INVOKABLE QString projectNameProblem(const QString &name, const QString &except) const;
    Q_INVOKABLE QString canvasNameProblem(const QString &project, const QString &name, const QString &except) const;
    Q_INVOKABLE bool renameProject(const QString &project, const QString &name);
    Q_INVOKABLE bool renameCanvas(const QString &project, const QString &canvas, const QString &name);

    Q_INVOKABLE void deleteProject(const QString &project);
    Q_INVOKABLE void deleteCanvas(const QString &project, const QString &canvas);

    Q_INVOKABLE void moveProject(const QString &project, int toIndex);
    Q_INVOKABLE void moveCanvas(const QString &project, const QString &canvas, const QString &toProject, int toIndex);
    Q_INVOKABLE void setExpanded(const QString &project, bool expanded);

signals:
    void currentChanged();
    void structureChanged();
    void filterChanged();
    // A new Project or Canvas was made: the UI lets the user name it right away.
    // `canvas` is empty for a Project.
    void renameRequested(const QString &project, const QString &canvas);

private:
    struct Row
    {
        int project;
        int canvas;   // -1 for a Project row
    };

    void rebuild();
    void expandedChanged(const QString &project);
    bool matches(const QString &name) const;

    Library *m_library;
    QVector<Row> m_rows;
    QString m_filter;
};
