#include "LibraryModel.h"

LibraryModel::LibraryModel(Library *library, QObject *parent)
    : QAbstractListModel(parent), m_library(library)
{
    connect(m_library, &Library::projectsChanged, this, &LibraryModel::rebuild);
    connect(m_library, &Library::expandedChanged, this, &LibraryModel::expandedChanged);
    connect(m_library, &Library::currentChanged, this, [this] {
        if (!m_rows.isEmpty())
            emit dataChanged(index(0), index(m_rows.size() - 1), {CurrentRole});
    });
}

// Rebuilt in one go after every change: the lists are short, and it keeps this simple.
void LibraryModel::rebuild()
{
    beginResetModel();
    m_rows.clear();
    const QVector<ProjectEntry> &projects = m_library->projects();
    for (int p = 0; p < projects.size(); ++p) {
        const ProjectEntry &project = projects[p];
        QVector<Row> canvases;
        const bool projectMatches = matches(project.name);
        for (int c = 0; c < project.canvases.size(); ++c) {
            if (m_filter.isEmpty() ? project.expanded : projectMatches || matches(project.canvases[c].name))
                canvases.append({p, c});
        }
        if (m_filter.isEmpty() || projectMatches || !canvases.isEmpty()) {
            m_rows.append({p, -1});
            m_rows += canvases;
        }
    }
    endResetModel();
}

// Expanding or collapsing only inserts or removes that Project's rows, so the rest of the list
// (and a row being clicked or double-clicked) stays as it is.
void LibraryModel::expandedChanged(const QString &project)
{
    if (!m_filter.isEmpty())
        return;   // while searching, every Project is shown expanded anyway
    const int header = rowOf(project);
    const int p = m_library->projectIndex(project);
    if (header < 0)
        return;
    const int count = int(m_library->projects()[p].canvases.size());
    const bool shown = header + 1 < m_rows.size() && m_rows[header + 1].project == p;
    if (m_library->projects()[p].expanded && !shown) {
        beginInsertRows({}, header + 1, header + count);
        for (int c = 0; c < count; ++c)
            m_rows.insert(header + 1 + c, {p, c});
        endInsertRows();
    } else if (!m_library->projects()[p].expanded && shown) {
        beginRemoveRows({}, header + 1, header + count);
        m_rows.remove(header + 1, count);
        endRemoveRows();
    }
    const QModelIndex changed = index(header);
    emit dataChanged(changed, changed, {ExpandedRole});
}

bool LibraryModel::matches(const QString &name) const
{
    return name.contains(m_filter.trimmed(), Qt::CaseInsensitive);
}

void LibraryModel::setFilter(const QString &filter)
{
    if (filter == m_filter)
        return;
    m_filter = filter;
    emit filterChanged();
    rebuild();
}

int LibraryModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant LibraryModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    const Row row = m_rows[index.row()];
    const ProjectEntry &project = m_library->projects()[row.project];
    const bool isProject = row.canvas < 0;
    switch (role) {
    case KindRole:
        return isProject ? QStringLiteral("project") : QStringLiteral("canvas");
    case NameRole:
        return isProject ? project.name : project.canvases[row.canvas].name;
    case ProjectRole:
        return project.name;
    case ProjectIndexRole:
        return row.project;
    case CanvasIndexRole:
        return row.canvas;
    case CanvasCountRole:
        return int(project.canvases.size());
    case ExpandedRole:
        return project.expanded || !m_filter.isEmpty();
    case CurrentRole:
        return project.name == m_library->currentProject() && (isProject || project.canvases[row.canvas].name == m_library->currentCanvas());
    }
    return {};
}

QHash<int, QByteArray> LibraryModel::roleNames() const
{
    return {
        {KindRole, "kind"},
        {NameRole, "name"},
        {ProjectRole, "project"},
        {ProjectIndexRole, "projectIndex"},
        {CanvasIndexRole, "canvasIndex"},
        {CanvasCountRole, "canvasCount"},
        {ExpandedRole, "expanded"},
        {CurrentRole, "current"},
    };
}

QVariantMap LibraryModel::rowAt(int row) const
{
    QVariantMap map;
    if (row < 0 || row >= m_rows.size())
        return map;
    const QHash<int, QByteArray> roles = roleNames();
    for (auto it = roles.begin(); it != roles.end(); ++it)
        map[QString::fromUtf8(it.value())] = data(index(row), it.key());
    return map;
}

int LibraryModel::rowOf(const QString &project, const QString &canvas) const
{
    const int p = m_library->projectIndex(project);
    const int c = canvas.isEmpty() ? -1 : m_library->canvasIndex(project, canvas);
    if (p < 0 || (!canvas.isEmpty() && c < 0))
        return -1;   // no such Project or Canvas (not its Project's row)
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].project == p && m_rows[i].canvas == c)
            return i;
    }
    return -1;
}

int LibraryModel::lastRowOf(const QString &project) const
{
    const int p = m_library->projectIndex(project);
    int last = -1;
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].project == p)
            last = i;
    }
    return last;
}

// ---- Actions ----

void LibraryModel::open(const QString &project, const QString &canvas)
{
    m_library->open(project, canvas.isEmpty() ? m_library->lastCanvas(project) : canvas);
}

void LibraryModel::openAdjacent(int step)
{
    const int p = m_library->projectIndex(m_library->currentProject());
    if (p < 0)
        return;
    const QVector<CanvasEntry> &canvases = m_library->projects()[p].canvases;
    const int next = m_library->canvasIndex(m_library->currentProject(), m_library->currentCanvas()) + step;
    if (next >= 0 && next < canvases.size())
        m_library->open(m_library->currentProject(), canvases[next].name);
}

bool LibraryModel::openFirstMatch()
{
    for (const Row &row : std::as_const(m_rows)) {
        if (row.canvas >= 0) {
            const ProjectEntry &project = m_library->projects()[row.project];
            m_library->open(project.name, project.canvases[row.canvas].name);
            return true;
        }
    }
    return false;
}

void LibraryModel::createProject()
{
    setFilter({});   // so the new row can be seen
    const QString name = m_library->createProject();
    if (!name.isEmpty())
        emit renameRequested(name, {});
}

void LibraryModel::createCanvas(const QString &project)
{
    setFilter({});
    const QString name = m_library->createCanvas(project);
    if (!name.isEmpty())
        emit renameRequested(project, name);
}

void LibraryModel::importCanvases(const QList<QUrl> &urls)
{
    for (const QUrl &url : urls) {
        if (url.isLocalFile())
            m_library->importCanvas(url.toLocalFile(), m_library->currentProject());
    }
}
