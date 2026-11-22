#include "LibraryRows.h"

LibraryRows::LibraryRows(Library *library, QObject *parent)
    : QAbstractListModel(parent), m_library(library)
{
    connect(m_library, &Library::projectsChanged, this, &LibraryRows::rebuild);
    connect(m_library, &Library::createdEmpty, this, [this] { setFilter({}); });   // so the new row can be seen
    connect(m_library, &Library::expandedChanged, this, &LibraryRows::showExpanded);
    connect(m_library, &Library::currentChanged, this, [this] {
        if (!m_rows.isEmpty())
            emit dataChanged(index(0), index(m_rows.size() - 1), {CurrentRole});
    });
}

// Rebuilt in one go after every change: the lists are short, and it keeps this simple.
void LibraryRows::rebuild()
{
    beginResetModel();
    m_rows.clear();
    const QVector<ProjectEntry> &projects = m_library->projects();
    for (int p = 0; p < projects.size(); ++p) {
        const ProjectEntry &project = projects[p];
        QVector<Row> canvases;
        const bool projectMatches = matches(project.name);
        for (int c = 0; c < project.canvases.size(); ++c) {
            // Without a search, the open Projects show their Canvases; with one, the matching
            // Canvases, or all of a matching Project's.
            const bool shown = m_filter.isEmpty() ? project.expanded
                                                  : projectMatches || matches(project.canvases[c].name);
            if (shown)
                canvases.append({p, c});
        }
        if (m_filter.isEmpty() || projectMatches || !canvases.isEmpty()) {
            m_rows.append({p, std::nullopt});
            m_rows += canvases;
        }
    }
    endResetModel();
}

// Expanding or collapsing only inserts or removes that Project's rows, so the rest of the list
// (and a row being clicked or double-clicked) stays as it is.
void LibraryRows::showExpanded(const QString &project)
{
    if (!m_filter.isEmpty())
        return;   // while searching, every Project is shown expanded anyway
    const int header = rowOf(project);
    const int p = m_library->projectIndex(project);
    if (header < 0)
        return;
    const int count = int(m_library->projects()[p].canvases.size());
    const bool shown = header + 1 < m_rows.size() && m_rows[header + 1].projectIndex == p;
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

bool LibraryRows::matches(const QString &name) const
{
    return name.contains(m_filter.trimmed(), Qt::CaseInsensitive);
}

void LibraryRows::setFilter(const QString &filter)
{
    if (filter == m_filter)
        return;
    m_filter = filter;
    emit filterChanged();
    rebuild();
}

int LibraryRows::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_rows.size();
}

QVariant LibraryRows::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= m_rows.size())
        return {};
    // The rows are rebuilt on every projectsChanged; one that no longer matches the Library
    // (which would be a bug) is shown as nothing rather than read out of range.
    const Row row = m_rows[index.row()];
    if (row.projectIndex >= m_library->projects().size())
        return {};
    const ProjectEntry &project = m_library->projects()[row.projectIndex];
    if (row.canvasIndex && *row.canvasIndex >= project.canvases.size())
        return {};
    const bool isProject = !row.canvasIndex;
    switch (role) {
    case IsProjectRole:
        return isProject;
    case NameRole:
        return isProject ? project.name : project.canvases[*row.canvasIndex].name;
    case ProjectRole:
        return project.name;
    case ProjectIndexRole:
        return row.projectIndex;
    case CanvasIndexRole:
        return row.canvasIndex.value_or(-1);   // QML reads -1 for a Project row
    case CanvasCountRole:
        return int(project.canvases.size());
    case ExpandedRole:
        return project.expanded || !m_filter.isEmpty();
    case CurrentRole:
        return project.name == m_library->currentProject() && (isProject || project.canvases[*row.canvasIndex].name == m_library->currentCanvas());
    }
    return {};
}

QHash<int, QByteArray> LibraryRows::roleNames() const
{
    return {
        {IsProjectRole, "isProject"},
        {NameRole, "name"},
        {ProjectRole, "project"},
        {ProjectIndexRole, "projectIndex"},
        {CanvasIndexRole, "canvasIndex"},
        {CanvasCountRole, "canvasCount"},
        {ExpandedRole, "expanded"},
        {CurrentRole, "current"},
    };
}

QVariantMap LibraryRows::rowAt(int row) const
{
    QVariantMap map;
    if (row < 0 || row >= m_rows.size())
        return map;
    const QHash<int, QByteArray> roles = roleNames();
    for (auto it = roles.begin(); it != roles.end(); ++it)
        map[QString::fromUtf8(it.value())] = data(index(row), it.key());
    return map;
}

int LibraryRows::rowOf(const QString &project, const QString &canvas) const
{
    const int p = m_library->projectIndex(project);
    if (p < 0)
        return -1;
    std::optional<int> c;   // none: the Project's own row
    if (!canvas.isEmpty()) {
        c = m_library->canvasIndex(project, canvas);
        if (*c < 0)
            return -1;   // no such Canvas (not its Project's row instead)
    }
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].projectIndex == p && m_rows[i].canvasIndex == c)
            return i;
    }
    return -1;
}

int LibraryRows::lastRowOf(const QString &project) const
{
    const int p = m_library->projectIndex(project);
    int last = -1;
    for (int i = 0; i < m_rows.size(); ++i) {
        if (m_rows[i].projectIndex == p)
            last = i;
    }
    return last;
}

bool LibraryRows::openFirstMatch()
{
    for (const Row &row : std::as_const(m_rows)) {
        if (row.canvasIndex) {
            const ProjectEntry &project = m_library->projects()[row.projectIndex];
            m_library->open(project.name, project.canvases[*row.canvasIndex].name);
            return true;
        }
    }
    return false;
}
