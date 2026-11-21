#include "Library.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace {

const QString IndexFile = QStringLiteral(".junction");
const QString Extension = QStringLiteral(".canvas");
const QString NewName = QStringLiteral("Untitled");
constexpr auto SortOrder = QDir::Name | QDir::IgnoreCase | QDir::LocaleAware;

bool sameName(const QString &a, const QString &b)
{
    return a.compare(b, Qt::CaseInsensitive) == 0;
}

QJsonObject viewToJson(const CanvasView &view)
{
    return {{"x", view.center.x()}, {"y", view.center.y()}, {"zoom", view.zoom}};
}

std::optional<CanvasView> viewFromJson(const QJsonValue &value)
{
    const QJsonObject json = value.toObject();
    const qreal zoom = json["zoom"].toDouble();
    if (zoom <= 0)
        return std::nullopt;
    return CanvasView{QPointF(json["x"].toDouble(), json["y"].toDouble()), zoom};
}

} // namespace

Library::Library(const QString &root, QObject *parent)
    : QObject(parent), m_root(root)
{
}

Library::~Library()
{
    if (!m_projects.isEmpty())
        saveIndex();
}

// ---- Loading and saving ----

void Library::load()
{
    QDir().mkpath(m_root);

    // What the index remembers: order, views, expanded state, the open Canvas.
    QFile indexFile(QDir(m_root).filePath(IndexFile));
    QJsonObject index;
    if (indexFile.open(QIODevice::ReadOnly))
        index = QJsonDocument::fromJson(indexFile.readAll()).object();

    QVector<ProjectEntry> listed;
    for (const QJsonValue &value : index["projects"].toArray()) {
        const QJsonObject json = value.toObject();
        ProjectEntry project{json["name"].toString(), {}, json["expanded"].toBool(true), json["last"].toString()};
        for (const QJsonValue &canvas : json["canvases"].toArray())
            project.canvases.append({canvas["name"].toString(), viewFromJson(canvas["view"])});
        listed.append(project);
    }

    // What is really on disk wins: listed items keep their order, unlisted ones go last.
    const QStringList folders = QDir(m_root).entryList(QDir::Dirs | QDir::NoDotAndDotDot, SortOrder);
    QVector<ProjectEntry> found;
    for (const ProjectEntry &p : listed) {
        if (folders.contains(p.name) && std::none_of(found.begin(), found.end(), [&](auto &f) { return f.name == p.name; }))
            found.append(p);
    }
    for (const QString &folder : folders) {
        if (std::none_of(found.begin(), found.end(), [&](auto &f) { return f.name == folder; }))
            found.append({folder, {}, true, {}});
    }

    m_projects.clear();
    for (ProjectEntry p : found) {
        QStringList files;
        for (const QString &file : QDir(projectPath(p.name)).entryList({"*" + Extension}, QDir::Files, SortOrder))
            files.append(file.chopped(Extension.size()));
        QVector<CanvasEntry> canvases;
        for (const CanvasEntry &c : p.canvases) {
            if (files.contains(c.name) && std::none_of(canvases.begin(), canvases.end(), [&](auto &k) { return k.name == c.name; }))
                canvases.append(c);
        }
        for (const QString &file : files) {
            if (std::none_of(canvases.begin(), canvases.end(), [&](auto &k) { return k.name == file; }))
                canvases.append({file, std::nullopt});
        }
        // A Project always has a Canvas to show; an empty folder gets a new one.
        if (canvases.isEmpty()) {
            if (!createEmptyCanvasFile(canvasPath(p.name, NewName)))
                continue;
            canvases.append({NewName, std::nullopt});
        }
        p.canvases = canvases;
        m_projects.append(p);
    }
    if (m_projects.isEmpty() && addProject(0).isEmpty())
        return;   // the folder can't be written; errorOccurred has said so

    const QJsonObject current = index["current"].toObject();
    const QString project = current["project"].toString();
    const QString canvas = current["canvas"].toString();
    if (canvasIndex(project, canvas) >= 0)
        setCurrent(project, canvas);
    else if (projectIndex(project) >= 0)
        setCurrent(project, m_projects[projectIndex(project)].canvases.first().name);
    else
        setCurrent(m_projects.first().name, m_projects.first().canvases.first().name);
    finish();
}

void Library::saveIndex()
{
    QJsonArray projects;
    for (const ProjectEntry &p : m_projects) {
        QJsonArray canvases;
        for (const CanvasEntry &c : p.canvases) {
            QJsonObject canvas{{"name", c.name}};
            if (c.view)
                canvas["view"] = viewToJson(*c.view);
            canvases.append(canvas);
        }
        projects.append(QJsonObject{{"name", p.name}, {"expanded", p.expanded}, {"last", p.lastCanvas}, {"canvases", canvases}});
    }
    const QJsonObject index{
        {"projects", projects},
        {"current", QJsonObject{{"project", m_currentProject}, {"canvas", m_currentCanvas}}},
    };
    QSaveFile file(QDir(m_root).filePath(IndexFile));
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(index).toJson(QJsonDocument::Indented));
        file.commit();
    }
}

void Library::finish()
{
    saveIndex();
    emit changed();
    if (m_currentMoved) {
        m_currentMoved = false;
        emit currentChanged();
    }
}

// ---- Looking things up ----

int Library::projectIndex(const QString &project) const
{
    for (int i = 0; i < m_projects.size(); ++i) {
        if (m_projects[i].name == project)
            return i;
    }
    return -1;
}

int Library::canvasIndex(const QString &project, const QString &canvas) const
{
    const int p = projectIndex(project);
    if (p < 0)
        return -1;
    const QVector<CanvasEntry> &canvases = m_projects[p].canvases;
    for (int i = 0; i < canvases.size(); ++i) {
        if (canvases[i].name == canvas)
            return i;
    }
    return -1;
}

ProjectEntry *Library::project(const QString &name)
{
    const int i = projectIndex(name);
    return i < 0 ? nullptr : &m_projects[i];
}

QString Library::projectPath(const QString &project) const
{
    return QDir(m_root).filePath(project);
}

QString Library::canvasPath(const QString &project, const QString &canvas) const
{
    if (project.isEmpty() || canvas.isEmpty())
        return {};
    return QDir(projectPath(project)).filePath(canvas + Extension);
}

QStringList Library::projectNames() const
{
    QStringList names;
    for (const ProjectEntry &p : m_projects)
        names.append(p.name);
    return names;
}

QStringList Library::canvasNames(const QString &project) const
{
    QStringList names;
    if (const int p = projectIndex(project); p >= 0) {
        for (const CanvasEntry &c : m_projects[p].canvases)
            names.append(c.name);
    }
    return names;
}

// ---- Names ----

QString Library::nameProblem(const QString &name)
{
    if (name.isEmpty())
        return tr("A name can't be empty.");
    if (name.startsWith('.') || name.endsWith('.'))
        return tr("A name can't start or end with a dot.");
    for (const QChar c : name) {
        if (c.category() == QChar::Other_Control || QStringLiteral("/\\:*?\"<>|").contains(c))
            return tr("A name can't contain / \\ : * ? \" < > |");
    }
    if (name.size() > 100)
        return tr("That name is too long.");
    return {};
}

QString Library::projectNameProblem(const QString &name, const QString &except) const
{
    const QString trimmed = name.trimmed();
    if (QString problem = nameProblem(trimmed); !problem.isEmpty())
        return problem;
    for (const ProjectEntry &p : m_projects) {
        if (p.name != except && sameName(p.name, trimmed))
            return tr("There is already a project named “%1”.").arg(p.name);
    }
    return {};
}

QString Library::canvasNameProblem(const QString &project, const QString &name, const QString &except) const
{
    const QString trimmed = name.trimmed();
    if (QString problem = nameProblem(trimmed); !problem.isEmpty())
        return problem;
    for (const QString &other : canvasNames(project)) {
        if (other != except && sameName(other, trimmed))
            return tr("“%1” already has a canvas named “%2”.").arg(project, other);
    }
    return {};
}

// "Untitled", "Untitled 2", "Untitled 3"...
QString Library::uniqueName(const QString &base, const QStringList &taken)
{
    QString name = base;
    for (int i = 2; std::any_of(taken.begin(), taken.end(), [&](const QString &t) { return sameName(t, name); }); ++i)
        name = QStringLiteral("%1 %2").arg(base).arg(i);
    return name;
}

// ---- The open Canvas ----

void Library::setCurrent(const QString &project, const QString &canvas)
{
    if (project == m_currentProject && canvas == m_currentCanvas)
        return;
    m_currentProject = project;
    m_currentCanvas = canvas;
    m_currentMoved = true;
    if (ProjectEntry *p = this->project(project))
        p->lastCanvas = canvas;
}

// Only the open Canvas changes here, not the list: no changed().
bool Library::open(const QString &projectName, const QString &canvas)
{
    if (canvasIndex(projectName, canvas) < 0)
        return false;
    if (projectName == m_currentProject && canvas == m_currentCanvas) {
        setExpanded(projectName, true);
        return true;
    }
    emit aboutToChange();
    setCurrent(projectName, canvas);
    ProjectEntry *p = project(projectName);
    const bool expand = !p->expanded;   // so the open Canvas can be seen in the sidebar
    p->expanded = true;
    saveIndex();
    if (expand)
        emit expandedChanged(projectName);
    m_currentMoved = false;
    emit currentChanged();
    return true;
}

QString Library::lastCanvas(const QString &projectName) const
{
    const int p = projectIndex(projectName);
    if (p < 0)
        return {};
    const ProjectEntry &project = m_projects[p];
    return canvasIndex(projectName, project.lastCanvas) >= 0 ? project.lastCanvas : project.canvases.first().name;
}

// ---- Creating ----

bool Library::createEmptyCanvasFile(const QString &path)
{
    QDir().mkpath(QFileInfo(path).absolutePath());
    QSaveFile file(path);
    const QJsonObject empty{{"nodes", QJsonArray()}, {"edges", QJsonArray()}};
    if (file.open(QIODevice::WriteOnly) && file.write(QJsonDocument(empty).toJson()) >= 0 && file.commit())
        return true;
    emit errorOccurred(tr("Could not create %1: %2").arg(path, file.errorString()));
    return false;
}

// A new Project folder with one empty Canvas, inserted at `at`.
QString Library::addProject(int at)
{
    const QString name = uniqueName(NewName, projectNames());
    if (!QDir(m_root).mkpath(name)) {
        emit errorOccurred(tr("Could not create the folder %1.").arg(projectPath(name)));
        return {};
    }
    if (!createEmptyCanvasFile(canvasPath(name, NewName))) {
        QDir(projectPath(name)).removeRecursively();
        return {};
    }
    m_projects.insert(at, {name, {{NewName, std::nullopt}}, true, {}});
    return name;
}

QString Library::createProject()
{
    emit aboutToChange();
    const QString name = addProject(m_projects.size());
    if (name.isEmpty())
        return {};
    setCurrent(name, NewName);
    finish();
    return name;
}

QString Library::createCanvas(const QString &projectName)
{
    ProjectEntry *p = project(projectName);
    if (!p)
        return {};
    emit aboutToChange();
    const QString name = uniqueName(NewName, canvasNames(projectName));
    if (!createEmptyCanvasFile(canvasPath(projectName, name)))
        return {};
    p->canvases.append({name, std::nullopt});
    p->expanded = true;
    setCurrent(projectName, name);
    finish();
    return name;
}

QString Library::duplicateCanvas(const QString &projectName, const QString &canvas)
{
    const int i = canvasIndex(projectName, canvas);
    if (i < 0)
        return {};
    emit aboutToChange();   // the open Canvas is saved first, so the copy has its latest state
    const QString name = uniqueName(tr("%1 copy").arg(canvas), canvasNames(projectName));
    if (!QFile::copy(canvasPath(projectName, canvas), canvasPath(projectName, name))) {
        emit errorOccurred(tr("Could not copy “%1”.").arg(canvas));
        return {};
    }
    ProjectEntry *p = project(projectName);
    CanvasEntry copy = p->canvases[i];
    copy.name = name;
    p->canvases.insert(i + 1, copy);
    setCurrent(projectName, name);
    finish();
    return name;
}

QString Library::duplicateProject(const QString &projectName)
{
    const int i = projectIndex(projectName);
    if (i < 0)
        return {};
    emit aboutToChange();
    const QString name = uniqueName(tr("%1 copy").arg(projectName), projectNames());
    ProjectEntry copy = m_projects[i];
    copy.name = name;
    bool copied = QDir(m_root).mkpath(name);
    for (const CanvasEntry &c : copy.canvases)
        copied = copied && QFile::copy(canvasPath(projectName, c.name), canvasPath(name, c.name));
    if (!copied) {
        QDir(projectPath(name)).removeRecursively();
        emit errorOccurred(tr("Could not copy “%1”.").arg(projectName));
        return {};
    }
    m_projects.insert(i + 1, copy);
    setCurrent(name, copy.canvases.first().name);
    finish();
    return name;
}

QString Library::importCanvas(const QString &filePath, const QString &projectName)
{
    ProjectEntry *p = project(projectName);
    if (!p)
        return {};
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        emit errorOccurred(tr("Could not open %1: %2").arg(filePath, file.errorString()));
        return {};
    }
    QJsonParseError parseError;
    const QJsonDocument json = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !json.isObject()) {
        emit errorOccurred(tr("%1 is not a canvas file.").arg(QFileInfo(filePath).fileName()));
        return {};
    }

    emit aboutToChange();
    QString base = QFileInfo(filePath).completeBaseName().trimmed();
    if (!nameProblem(base).isEmpty())
        base = tr("Imported canvas");
    const QString name = uniqueName(base, canvasNames(projectName));
    if (!QFile::copy(filePath, canvasPath(projectName, name))) {
        emit errorOccurred(tr("Could not copy %1 into the project.").arg(filePath));
        return {};
    }
    p->canvases.append({name, std::nullopt});
    p->expanded = true;
    setCurrent(projectName, name);
    finish();
    return name;
}

// ---- Renaming ----

// Like QDir::rename, but never replaces an existing file, and a change of case only
// ("auth" -> "Auth") also works where the file system ignores case.
bool Library::renamePath(const QString &from, const QString &to)
{
    if (sameName(from, to)) {
        const QString temporary = from + QStringLiteral(".renaming");
        if (!QDir().rename(from, temporary))
            return false;
        if (QDir().rename(temporary, to))
            return true;
        QDir().rename(temporary, from);
        return false;
    }
    return !QFileInfo::exists(to) && QDir().rename(from, to);
}

bool Library::renameProject(const QString &projectName, const QString &newName)
{
    const QString name = newName.trimmed();
    ProjectEntry *p = project(projectName);
    if (!p || name == projectName)
        return p != nullptr;
    if (QString problem = projectNameProblem(name, projectName); !problem.isEmpty()) {
        emit errorOccurred(problem);
        return false;
    }
    emit aboutToChange();
    if (!renamePath(projectPath(projectName), projectPath(name))) {
        emit errorOccurred(tr("Could not rename “%1” to “%2”.").arg(projectName, name));
        return false;
    }
    p->name = name;
    for (const CanvasEntry &c : p->canvases)
        emit canvasMoved(canvasPath(projectName, c.name), canvasPath(name, c.name));
    if (m_currentProject == projectName) {
        m_currentProject = name;
        m_currentMoved = true;
    }
    finish();
    return true;
}

bool Library::renameCanvas(const QString &projectName, const QString &canvas, const QString &newName)
{
    const QString name = newName.trimmed();
    const int i = canvasIndex(projectName, canvas);
    if (i < 0 || name == canvas)
        return i >= 0;
    if (QString problem = canvasNameProblem(projectName, name, canvas); !problem.isEmpty()) {
        emit errorOccurred(problem);
        return false;
    }
    emit aboutToChange();
    const QString from = canvasPath(projectName, canvas);
    const QString to = canvasPath(projectName, name);
    if (!renamePath(from, to)) {
        emit errorOccurred(tr("Could not rename “%1” to “%2”.").arg(canvas, name));
        return false;
    }
    ProjectEntry *p = project(projectName);
    p->canvases[i].name = name;
    if (p->lastCanvas == canvas)
        p->lastCanvas = name;
    emit canvasMoved(from, to);
    if (m_currentProject == projectName && m_currentCanvas == canvas) {
        m_currentCanvas = name;
        m_currentMoved = true;
    }
    finish();
    return true;
}

// ---- Deleting ----

bool Library::deleteCanvas(const QString &projectName, const QString &canvas)
{
    const int i = canvasIndex(projectName, canvas);
    if (i < 0)
        return false;
    ProjectEntry *p = project(projectName);
    if (p->canvases.size() == 1) {
        emit errorOccurred(tr("A project needs at least one canvas. To remove this one, delete the project."));
        return false;
    }
    emit aboutToChange();
    const QString path = canvasPath(projectName, canvas);
    if (QFileInfo::exists(path) && !QFile::moveToTrash(path)) {
        emit errorOccurred(tr("Could not move “%1” to the trash.").arg(canvas));
        return false;
    }
    p->canvases.removeAt(i);
    emit canvasRemoved(path);
    // The open Canvas is replaced by the one that took its place, or the one before it.
    if (m_currentProject == projectName && m_currentCanvas == canvas)
        setCurrent(projectName, p->canvases[qMin(i, p->canvases.size() - 1)].name);
    finish();
    return true;
}

bool Library::deleteProject(const QString &projectName)
{
    const int i = projectIndex(projectName);
    if (i < 0)
        return false;
    emit aboutToChange();
    const QString path = projectPath(projectName);
    if (QFileInfo::exists(path) && !QFile::moveToTrash(path)) {
        emit errorOccurred(tr("Could not move “%1” to the trash.").arg(projectName));
        return false;
    }
    const ProjectEntry removed = m_projects.takeAt(i);
    for (const CanvasEntry &c : removed.canvases)
        emit canvasRemoved(canvasPath(projectName, c.name));
    if (m_projects.isEmpty() && addProject(0).isEmpty()) {
        m_currentProject.clear();
        m_currentCanvas.clear();
        m_currentMoved = true;
        finish();
        return true;
    }
    if (m_currentProject == projectName) {
        const ProjectEntry &next = m_projects[qMin(i, m_projects.size() - 1)];
        setCurrent(next.name, next.canvases.first().name);
    }
    finish();
    return true;
}

// ---- Moving ----

void Library::moveProject(const QString &projectName, int toIndex)
{
    const int from = projectIndex(projectName);
    if (from < 0)
        return;
    int to = toIndex > from ? toIndex - 1 : toIndex;
    to = qBound(0, to, m_projects.size() - 1);
    if (to == from)
        return;
    m_projects.move(from, to);
    finish();
}

QString Library::moveCanvas(const QString &projectName, const QString &canvas, const QString &toProject, int toIndex)
{
    const int from = canvasIndex(projectName, canvas);
    ProjectEntry *source = project(projectName);
    ProjectEntry *target = project(toProject);
    if (from < 0 || !target)
        return {};

    if (source == target) {   // only the order changes
        int to = toIndex > from ? toIndex - 1 : toIndex;
        to = qBound(0, to, source->canvases.size() - 1);
        if (to != from) {
            source->canvases.move(from, to);
            finish();
        }
        return canvas;
    }

    if (source->canvases.size() == 1) {
        emit errorOccurred(tr("A project needs at least one canvas, so “%1” has to stay in “%2”.")
                               .arg(canvas, projectName));
        return {};
    }
    emit aboutToChange();
    const QString name = uniqueName(canvas, canvasNames(toProject));
    const QString oldPath = canvasPath(projectName, canvas);
    const QString newPath = canvasPath(toProject, name);
    if (!renamePath(oldPath, newPath)) {
        emit errorOccurred(tr("Could not move “%1” to “%2”.").arg(canvas, toProject));
        return {};
    }
    CanvasEntry entry = source->canvases.takeAt(from);
    entry.name = name;
    target->canvases.insert(qBound(0, toIndex, target->canvases.size()), entry);
    target->expanded = true;
    emit canvasMoved(oldPath, newPath);
    if (m_currentProject == projectName && m_currentCanvas == canvas) {
        m_currentProject = toProject;
        m_currentCanvas = name;
        m_currentMoved = true;
    }
    finish();
    return name;
}

// ---- Sidebar and view state ----

void Library::setExpanded(const QString &projectName, bool expanded)
{
    ProjectEntry *p = project(projectName);
    if (!p || p->expanded == expanded)
        return;
    p->expanded = expanded;
    saveIndex();
    emit expandedChanged(projectName);
}

std::optional<CanvasView> Library::view(const QString &projectName, const QString &canvas) const
{
    const int i = canvasIndex(projectName, canvas);
    if (i < 0)
        return std::nullopt;
    return m_projects[projectIndex(projectName)].canvases[i].view;
}

// Not written to disk right away: it changes on every pan step. The index is saved with the
// next change, and when the Library is closed.
void Library::setView(const QString &projectName, const QString &canvas, const CanvasView &view)
{
    const int i = canvasIndex(projectName, canvas);
    if (i >= 0)
        project(projectName)->canvases[i].view = view;
}
