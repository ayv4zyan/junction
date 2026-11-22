#pragma once

// The Library as the sidebar shows it: one flat list of rows, a Project row followed by its
// Canvas rows (while it is expanded). With a search filter, only matching rows are shown, all
// expanded. Also the sidebar's ways of opening, creating and importing that depend on those
// rows or the search; everything else QML asks of the Library directly.

#include "Library.h"

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QUrl>

// Library (in the core, which knows nothing about QML) as a QML type, for properties declared with it.
struct LibraryForeign
{
    Q_GADGET
    QML_FOREIGN(Library)
    QML_NAMED_ELEMENT(Library)
    QML_UNCREATABLE("Created by main.cpp")
};

class LibraryModel : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by main.cpp")

    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)

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

    QString filter() const { return m_filter; }
    void setFilter(const QString &filter);

    Q_INVOKABLE QVariantMap rowAt(int row) const;   // the roles of one row, by role name
    Q_INVOKABLE int rowOf(const QString &project, const QString &canvas = {}) const;
    Q_INVOKABLE int lastRowOf(const QString &project) const;   // its last visible Canvas row

    Q_INVOKABLE void open(const QString &project, const QString &canvas = {});   // no Canvas: the last one open there
    Q_INVOKABLE void openAdjacent(int step);   // the previous (-1) or next (1) Canvas in the Project
    Q_INVOKABLE bool openFirstMatch();          // the first Canvas the search shows

    // These clear the search, so the new row can be seen, and then ask for it to be named.
    Q_INVOKABLE void createProject();
    Q_INVOKABLE void createCanvas(const QString &project);
    Q_INVOKABLE void importCanvases(const QList<QUrl> &urls);   // into the current Project

signals:
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
