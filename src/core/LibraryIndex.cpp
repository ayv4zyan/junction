#include "LibraryIndex.h"

#include <QJsonArray>

namespace {

QJsonObject viewToJson(const Viewport &view)
{
    return {{"x", view.center.x()}, {"y", view.center.y()}, {"zoom", view.zoom}};
}

std::optional<Viewport> viewFromJson(const QJsonValue &value)
{
    const QJsonObject json = value.toObject();
    const qreal zoom = json["zoom"].toDouble();
    if (zoom <= 0)
        return std::nullopt;
    return Viewport{QPointF(json["x"].toDouble(), json["y"].toDouble()), zoom};
}

} // namespace

QJsonObject LibraryIndex::toJson() const
{
    QJsonArray projectsJson;
    for (const ProjectEntry &p : projects) {
        QJsonArray canvases;
        for (const CanvasEntry &c : p.canvases) {
            QJsonObject canvas{{"name", c.name}};
            if (c.view)
                canvas["view"] = viewToJson(*c.view);
            canvases.append(canvas);
        }
        projectsJson.append(QJsonObject{{"name", p.name}, {"expanded", p.expanded}, {"last", p.lastOpened}, {"canvases", canvases}});
    }
    return {
        {"projects", projectsJson},
        {"current", QJsonObject{{"project", currentProject}, {"canvas", currentCanvas}}},
    };
}

LibraryIndex LibraryIndex::fromJson(const QJsonObject &json)
{
    LibraryIndex index;
    for (const QJsonValue &value : json["projects"].toArray()) {
        const QJsonObject p = value.toObject();
        ProjectEntry project{p["name"].toString(), {}, p["expanded"].toBool(true), p["last"].toString()};
        for (const QJsonValue &canvas : p["canvases"].toArray())
            project.canvases.append({canvas["name"].toString(), viewFromJson(canvas["view"])});
        index.projects.append(project);
    }
    const QJsonObject current = json["current"].toObject();
    index.currentProject = current["project"].toString();
    index.currentCanvas = current["canvas"].toString();
    return index;
}

std::optional<int> LibraryIndex::projectPosition(const QString &project) const
{
    for (int i = 0; i < projects.size(); ++i) {
        if (projects[i].name == project)
            return i;
    }
    return std::nullopt;
}

std::optional<CanvasPosition> LibraryIndex::canvasPosition(const QString &project, const QString &canvas) const
{
    const std::optional<int> p = projectPosition(project);
    if (!p)
        return std::nullopt;
    const QVector<CanvasEntry> &canvases = projects[*p].canvases;
    for (int c = 0; c < canvases.size(); ++c) {
        if (canvases[c].name == canvas)
            return CanvasPosition{*p, c};
    }
    return std::nullopt;
}

ProjectEntry *LibraryIndex::project(const QString &name)
{
    const std::optional<int> p = projectPosition(name);
    return p ? &projects[*p] : nullptr;
}

const ProjectEntry *LibraryIndex::project(const QString &name) const
{
    const std::optional<int> p = projectPosition(name);
    return p ? &projects[*p] : nullptr;
}

QStringList LibraryIndex::projectNames() const
{
    QStringList names;
    for (const ProjectEntry &p : projects)
        names.append(p.name);
    return names;
}

QStringList LibraryIndex::canvasNames(const QString &project) const
{
    QStringList names;
    if (const ProjectEntry *p = this->project(project)) {
        for (const CanvasEntry &c : p->canvases)
            names.append(c.name);
    }
    return names;
}

QString LibraryIndex::lastOpenedCanvas(const QString &project) const
{
    const ProjectEntry *p = this->project(project);
    if (!p)
        return {};
    return canvasPosition(project, p->lastOpened) ? p->lastOpened : p->firstCanvas();
}
