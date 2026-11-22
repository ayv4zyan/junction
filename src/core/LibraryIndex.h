#pragma once

// The Library's index (".junction" in its folder): the order of Projects and Canvases, how each
// Canvas was last viewed, which Projects are shown open, and the Canvas that was open. Read
// and written as JSON by Library; the folders on disk stay the truth about what exists.

#include <QJsonObject>
#include <QPointF>
#include <QString>
#include <QVector>
#include <optional>

// What part of a Canvas was on screen: the world point at the center of the view, and the zoom.
struct Viewport
{
    Q_GADGET
    Q_PROPERTY(QPointF center MEMBER center)
    Q_PROPERTY(qreal zoom MEMBER zoom)

public:
    QPointF center;
    qreal zoom = 1;
};

struct CanvasEntry
{
    QString name;
    std::optional<Viewport> view;   // empty until the user pans or zooms it
};

struct ProjectEntry
{
    QString name;
    QVector<CanvasEntry> canvases;   // never empty in the Library (it adds one where there is none)
    bool expanded = true;            // shown open in the sidebar
    QString lastCanvas;              // the Canvas last open in this Project

    QString firstCanvas() const
    {
        Q_ASSERT(!canvases.isEmpty());
        return canvases.first().name;
    }
};

struct LibraryIndex
{
    QVector<ProjectEntry> projects;
    QString currentProject;
    QString currentCanvas;

    QJsonObject toJson() const;
    static LibraryIndex fromJson(const QJsonObject &json);   // missing parts are left empty
};
