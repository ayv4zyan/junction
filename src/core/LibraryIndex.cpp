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
