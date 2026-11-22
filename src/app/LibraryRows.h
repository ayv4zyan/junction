#pragma once

// The Library as the sidebar shows it: one flat list of rows, a Project row followed by its
// Canvas rows (while it is expanded). With a search filter, only matching rows are shown, all
// expanded. QML changes the Library itself; this only shows it.

#include "Library.h"

#include <QAbstractListModel>
#include <optional>
#include <QQmlEngine>

class LibraryRows : public QAbstractListModel
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by main.cpp")

    Q_PROPERTY(QString filter READ filter WRITE setFilter NOTIFY filterChanged)

public:
    enum Role {
        IsProjectRole = Qt::UserRole + 1,   // a Project row; otherwise a Canvas row
        NameRole,
        ProjectRole,          // the Project's name (for Canvas rows: the Project it is in)
        ProjectIndexRole,     // position of that Project in the Library
        CanvasIndexRole,      // position of the Canvas in its Project, -1 for Project rows
        CanvasCountRole,      // how many Canvases the Project has
        ExpandedRole,
        CurrentRole,          // the open Canvas, or the Project it is in
    };

    explicit LibraryRows(Library *library, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    QString filter() const { return m_filter; }
    void setFilter(const QString &filter);

    Q_INVOKABLE QVariantMap rowAt(int row) const;   // the roles of one row, by role name
    Q_INVOKABLE int rowOf(const QString &project, const QString &canvas = {}) const;
    Q_INVOKABLE int lastRowOf(const QString &project) const;   // its last visible Canvas row

    Q_INVOKABLE bool openFirstMatch();   // opens the first Canvas the search shows, if any

signals:
    void filterChanged();

private:
    struct Row
    {
        int projectIndex;                 // position in the Library
        std::optional<int> canvasIndex;   // position in the Project; none for the Project's own row
    };

    void rebuild();
    void expandedChanged(const QString &project);
    bool matches(const QString &name) const;

    Library *m_library;
    QVector<Row> m_rows;
    QString m_filter;
};
