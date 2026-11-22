#include "Library.h"

#include "Document.h"

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

// What is really on disk wins over what the index lists: the listed entries that exist keep
// their order, and names on disk that weren't listed follow, made with `newEntry`.
template <typename Entry, typename NewEntry>
QVector<Entry> reconciled(const QVector<Entry> &listed, const QStringList &onDisk, NewEntry newEntry)
{
    QVector<Entry> result;
    QStringList names;
    for (const Entry &entry : listed) {
        if (onDisk.contains(entry.name) && !names.contains(entry.name)) {
            result.append(entry);
            names.append(entry.name);
        }
    }
    for (const QString &name : onDisk) {
        if (!names.contains(name))
            result.append(newEntry(name));
    }
    return result;
}

// Where an item at `from` lands when dropped at `toIndex`, a position in the list of `count`
// items as it is before the item is taken out.
int movedPosition(int from, int toIndex, int count)
{
    const int to = toIndex > from ? toIndex - 1 : toIndex;
    return qBound(0, to, count - 1);
}

} // namespace

Library::Library(const QString &root, QObject *parent)
    : QObject(parent), m_root(root)
{
    m_viewSaveTimer.setSingleShot(true);
    m_viewSaveTimer.setInterval(1000);
    connect(&m_viewSaveTimer, &QTimer::timeout, this, &Library::saveIndex);
}

// Saves the index one last time. Nobody is listening for errorOccurred any more, so a failure
// goes to the log.
Library::~Library()
{
    if (!m_projects.isEmpty() && !writeIndex())
        qWarning("Junction: the library index could not be saved");
}

// ---- Loading and saving ----

void Library::load()
{
    if (const std::optional<LibraryIndex> index = loadProjects()) {
        openRemembered(index->currentProject, index->currentCanvas);
        saveIndexAndNotify();
    }
    emit loaded();
}

// Reads the index and the folders into m_projects; what is really on disk wins. Returns the
// index for what was open last time, or nothing when the folder can't be written
// (errorOccurred has said so).
std::optional<LibraryIndex> Library::loadProjects()
{
    if (!QDir().mkpath(m_root)) {
        emit errorOccurred(tr("Could not create the library folder %1.").arg(m_root));
        return std::nullopt;
    }
    const LibraryIndex index = readIndex();
    m_projects.clear();
    const QStringList folders = QDir(m_root).entryList(QDir::Dirs | QDir::NoDotAndDotDot, SortOrder);
    const auto newProject = [](const QString &name) { return ProjectEntry{name, {}, true, {}}; };
    const auto newCanvas = [](const QString &name) { return CanvasEntry{name, std::nullopt}; };
    for (ProjectEntry p : reconciled(index.projects, folders, newProject)) {
        p.canvases = reconciled(p.canvases, canvasFiles(p.name), newCanvas);
        // A Project always has a Canvas to show; an empty folder gets a new one.
        if (p.canvases.isEmpty()) {
            if (!createEmptyCanvasFile(canvasPath(p.name, NewName)))
                continue;
            p.canvases.append(newCanvas(NewName));
        }
        m_projects.append(p);
    }
    if (m_projects.isEmpty() && addProject(0).isEmpty())
        return std::nullopt;
    return index;
}

// The index: order, views, expanded state, the open Canvas. Empty the first time. An index
// that can't be read is kept as ".junction.broken" (load() is about to write a new one), and
// the user is told that the order and views were reset.
LibraryIndex Library::readIndex()
{
    const QString path = QDir(m_root).filePath(IndexFile);
    QFile file(path);
    if (!file.exists())
        return {};
    const QJsonDocument json = file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()) : QJsonDocument();
    if (json.isObject())
        return LibraryIndex::fromJson(json.object());
    file.close();
    const QString broken = path + QStringLiteral(".broken");
    const bool kept = (!QFile::exists(broken) || QFile::remove(broken)) && QFile::copy(path, broken);
    const QString reset = tr("The library index couldn't be read, so the order of projects and canvases was reset.");
    emit errorOccurred(kept ? reset + ' ' + tr("The old index was kept as %1.").arg(broken) : reset);
    return {};
}

// The Canvas open last time; if it is gone, the first of its Project, or else the very first.
void Library::openRemembered(const QString &project, const QString &canvas)
{
    if (canvasIndex(project, canvas) >= 0)
        setCurrent(project, canvas);
    else if (projectIndex(project) >= 0)
        setCurrent(project, lastCanvas(project));
    else
        setCurrent(m_projects.first().name, m_projects.first().firstCanvas());
}

QStringList Library::canvasFiles(const QString &project) const
{
    QStringList names;
    for (const QString &file : QDir(projectPath(project)).entryList({"*" + Extension}, QDir::Files, SortOrder))
        names.append(file.chopped(Extension.size()));
    return names;
}

void Library::saveIndex()
{
    if (!writeIndex())
        emit errorOccurred(tr("Could not save the library index in %1.").arg(m_root));
}

bool Library::writeIndex() const
{
    const LibraryIndex index{m_projects, m_currentProject, m_currentCanvas};
    QSaveFile file(QDir(m_root).filePath(IndexFile));
    return file.open(QIODevice::WriteOnly)
           && file.write(QJsonDocument(index.toJson()).toJson(QJsonDocument::Indented)) >= 0 && file.commit();
}

void Library::saveIndexAndNotify()
{
    saveIndex();
    emit projectsChanged();
    announceCurrent();
}

void Library::announceCurrent()
{
    if (currentPath() == m_lastAnnouncedPath)
        return;
    m_lastAnnouncedPath = currentPath();
    emit currentChanged();
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

std::optional<Library::FoundCanvas> Library::findCanvas(const QString &project, const QString &canvas) const
{
    const int p = projectIndex(project);
    const int c = canvasIndex(project, canvas);
    if (p < 0 || c < 0)
        return std::nullopt;
    return FoundCanvas{p, c};
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

int Library::canvasCount(const QString &project) const
{
    const int p = projectIndex(project);
    return p < 0 ? 0 : int(m_projects[p].canvases.size());
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
    const auto taken = [](const QString &existing) { return tr("There is already a project named “%1”.").arg(existing); };
    return nameProblemAmong(name.trimmed(), projectNames(), except, taken);
}

QString Library::canvasNameProblem(const QString &project, const QString &name, const QString &except) const
{
    const auto taken = [&](const QString &existing) { return tr("“%1” already has a canvas named “%2”.").arg(project, existing); };
    return nameProblemAmong(name.trimmed(), canvasNames(project), except, taken);
}

// The name's problem (see nameProblem()), or the message `taken` makes for an existing name it
// equals, ignoring case. `except`, the item being renamed, doesn't count.
QString Library::nameProblemAmong(const QString &name, const QStringList &existing, const QString &except,
                                  const std::function<QString(const QString &)> &taken)
{
    if (QString problem = nameProblem(name); !problem.isEmpty())
        return problem;
    for (const QString &other : existing) {
        if (other != except && sameName(other, name))
            return taken(other);
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
    if (ProjectEntry *p = this->project(project))
        p->lastCanvas = canvas;
}

// Only the open Canvas changes here, not the list: no projectsChanged().
bool Library::open(const QString &projectName, const QString &canvasName)
{
    const QString canvas = canvasName.isEmpty() ? lastCanvas(projectName) : canvasName;
    if (canvasIndex(projectName, canvas) < 0)
        return false;
    if (projectName == m_currentProject && canvas == m_currentCanvas) {
        setExpanded(projectName, true);
        return true;
    }
    emit aboutToChangeFiles();
    setCurrent(projectName, canvas);
    ProjectEntry *p = project(projectName);
    const bool expand = !p->expanded;   // so the open Canvas can be seen in the sidebar
    p->expanded = true;
    saveIndex();
    if (expand)
        emit expandedChanged(projectName);
    announceCurrent();
    return true;
}

void Library::openAdjacent(int step)
{
    const int p = projectIndex(m_currentProject);
    if (p < 0)
        return;
    const int next = canvasIndex(m_currentProject, m_currentCanvas) + step;
    if (next >= 0 && next < m_projects[p].canvases.size())
        open(m_currentProject, m_projects[p].canvases[next].name);
}

QString Library::lastCanvas(const QString &projectName) const
{
    const int p = projectIndex(projectName);
    if (p < 0)
        return {};
    const ProjectEntry &project = m_projects[p];
    return canvasIndex(projectName, project.lastCanvas) >= 0 ? project.lastCanvas : project.firstCanvas();
}

// ---- Creating ----

bool Library::createEmptyCanvasFile(const QString &path)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath())) {
        emit errorOccurred(tr("Could not create the folder for %1.").arg(path));
        return false;
    }
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

// The last step of every way of adding a Canvas: it goes into the Project at `at`, is shown,
// and is opened.
QString Library::addCanvas(ProjectEntry *p, int at, const CanvasEntry &entry)
{
    p->canvases.insert(at, entry);
    p->expanded = true;
    setCurrent(p->name, entry.name);
    saveIndexAndNotify();
    return entry.name;
}

QString Library::createProject()
{
    emit aboutToChangeFiles();
    const QString name = addProject(m_projects.size());
    if (name.isEmpty())
        return {};
    setCurrent(name, NewName);
    saveIndexAndNotify();
    emit created(name, {});
    return name;
}

QString Library::createCanvas(const QString &projectName)
{
    ProjectEntry *p = project(projectName);
    if (!p)
        return {};
    emit aboutToChangeFiles();
    const QString name = uniqueName(NewName, canvasNames(projectName));
    if (!createEmptyCanvasFile(canvasPath(projectName, name)))
        return {};
    addCanvas(p, p->canvases.size(), {name, std::nullopt});
    emit created(projectName, name);
    return name;
}

QString Library::duplicateCanvas(const QString &projectName, const QString &canvas)
{
    const auto at = findCanvas(projectName, canvas);
    if (!at)
        return {};
    emit aboutToChangeFiles();   // the open Canvas is saved first, so the copy has its latest state
    const QString name = uniqueName(tr("%1 copy").arg(canvas), canvasNames(projectName));
    if (!QFile::copy(canvasPath(projectName, canvas), canvasPath(projectName, name))) {
        emit errorOccurred(tr("Could not copy “%1”.").arg(canvas));
        return {};
    }
    ProjectEntry &p = m_projects[at->projectIndex];
    CanvasEntry copy = p.canvases[at->canvasIndex];
    copy.name = name;
    return addCanvas(&p, at->canvasIndex + 1, copy);
}

QString Library::duplicateProject(const QString &projectName)
{
    const int i = projectIndex(projectName);
    if (i < 0)
        return {};
    emit aboutToChangeFiles();
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
    setCurrent(name, copy.firstCanvas());
    saveIndexAndNotify();
    return name;
}

QString Library::importCanvas(const QString &filePath, const QString &projectName)
{
    ProjectEntry *p = project(projectName);
    if (!p)
        return {};
    Document probe;   // only files that open as a Canvas are copied in
    if (QString error; !probe.load(filePath, &error)) {
        emit errorOccurred(tr("%1 can't be imported: %2").arg(QFileInfo(filePath).fileName(), error));
        return {};
    }

    emit aboutToChangeFiles();
    QString base = QFileInfo(filePath).completeBaseName().trimmed();
    if (!nameProblem(base).isEmpty())
        base = tr("Imported canvas");
    const QString name = uniqueName(base, canvasNames(projectName));
    if (!QFile::copy(filePath, canvasPath(projectName, name))) {
        emit errorOccurred(tr("Could not copy %1 into the project.").arg(filePath));
        return {};
    }
    return addCanvas(p, p->canvases.size(), {name, std::nullopt});
}

void Library::importCanvases(const QList<QUrl> &files)
{
    for (const QUrl &file : files) {
        if (file.isLocalFile())
            importCanvas(file.toLocalFile(), m_currentProject);
    }
}

// ---- Renaming ----

// Like QDir::rename, but never replaces an existing file, and a change of case only
// ("auth" -> "Auth") also works where the file system ignores case.
bool Library::renamePath(const QString &from, const QString &to, const QString &failure)
{
    QString stranded;   // where the file ended up when it couldn't be put back
    if (moveFile(from, to, &stranded))
        return true;
    if (!failure.isEmpty())
        emit errorOccurred(stranded.isEmpty() ? failure
                                              : failure + ' ' + tr("It could not be renamed back either, and is now called %1.").arg(stranded));
    return false;
}

// The rename itself. A change of case only goes through a temporary name, since a file system
// that ignores case sees "auth" and "Auth" as the same file.
bool Library::moveFile(const QString &from, const QString &to, QString *stranded)
{
    if (!sameName(from, to))
        return !QFileInfo::exists(to) && QDir().rename(from, to);
    const QString temporary = from + QStringLiteral(".renaming");
    if (!QDir().rename(from, temporary))
        return false;
    if (QDir().rename(temporary, to))
        return true;
    if (!QDir().rename(temporary, from))
        *stranded = temporary;
    return false;
}

bool Library::renameProject(const QString &projectName, const QString &newName)
{
    const QString name = newName.trimmed();
    ProjectEntry *p = project(projectName);
    if (!p)
        return false;
    if (name == projectName)
        return true;   // nothing to do
    if (QString problem = projectNameProblem(name, projectName); !problem.isEmpty()) {
        emit errorOccurred(problem);
        return false;
    }
    emit aboutToChangeFiles();
    if (!renamePath(projectPath(projectName), projectPath(name), tr("Could not rename “%1” to “%2”.").arg(projectName, name)))
        return false;
    p->name = name;
    for (const CanvasEntry &c : p->canvases)
        emit canvasMoved(canvasPath(projectName, c.name), canvasPath(name, c.name));
    if (m_currentProject == projectName)
        m_currentProject = name;
    saveIndexAndNotify();
    return true;
}

bool Library::renameCanvas(const QString &projectName, const QString &canvas, const QString &newName)
{
    const QString name = newName.trimmed();
    const auto at = findCanvas(projectName, canvas);
    if (!at)
        return false;
    if (name == canvas)
        return true;   // nothing to do
    if (QString problem = canvasNameProblem(projectName, name, canvas); !problem.isEmpty()) {
        emit errorOccurred(problem);
        return false;
    }
    emit aboutToChangeFiles();
    const QString from = canvasPath(projectName, canvas);
    const QString to = canvasPath(projectName, name);
    if (!renamePath(from, to, tr("Could not rename “%1” to “%2”.").arg(canvas, name)))
        return false;
    ProjectEntry &p = m_projects[at->projectIndex];
    p.canvases[at->canvasIndex].name = name;
    if (p.lastCanvas == canvas)
        p.lastCanvas = name;
    emit canvasMoved(from, to);
    if (m_currentProject == projectName && m_currentCanvas == canvas)
        m_currentCanvas = name;
    saveIndexAndNotify();
    return true;
}

// ---- Deleting ----

bool Library::deleteCanvas(const QString &projectName, const QString &canvas)
{
    const auto at = findCanvas(projectName, canvas);
    if (!at)
        return false;
    ProjectEntry *p = &m_projects[at->projectIndex];
    const int i = at->canvasIndex;
    if (p->canvases.size() == 1) {
        emit errorOccurred(tr("A project needs at least one canvas. To remove this one, delete the project."));
        return false;
    }
    emit aboutToChangeFiles();
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
    saveIndexAndNotify();
    return true;
}

bool Library::deleteProject(const QString &projectName)
{
    const int i = projectIndex(projectName);
    if (i < 0)
        return false;
    emit aboutToChangeFiles();
    if (m_projects.size() == 1)
        return replaceLastProject();
    if (!trashProject(i))
        return false;
    if (m_currentProject == projectName) {   // the Project that took its place, or the one before
        const ProjectEntry &next = m_projects[qMin(i, m_projects.size() - 1)];
        setCurrent(next.name, next.firstCanvas());
    }
    saveIndexAndNotify();
    return true;
}

// There is always a Project, so the last one is replaced by a new empty one. That one is made
// first, so that where the folder can't be written nothing is deleted. It gets a free name
// ("Untitled 2"), and takes "Untitled" once the old one is gone, if it can.
bool Library::replaceLastProject()
{
    const QString replacement = addProject(1);
    if (replacement.isEmpty())
        return false;   // errorOccurred has said why
    if (!trashProject(0)) {
        QDir(projectPath(replacement)).removeRecursively();
        m_projects.removeLast();
        return false;
    }
    ProjectEntry &project = m_projects.first();
    if (project.name != NewName && renamePath(projectPath(project.name), projectPath(NewName), {}))
        project.name = NewName;
    setCurrent(project.name, project.firstCanvas());
    saveIndexAndNotify();
    return true;
}

// Moves the Project's folder to the trash and takes it out of the list.
bool Library::trashProject(int index)
{
    const QString name = m_projects[index].name;
    const QString path = projectPath(name);
    if (QFileInfo::exists(path) && !QFile::moveToTrash(path)) {
        emit errorOccurred(tr("Could not move “%1” to the trash.").arg(name));
        return false;
    }
    for (const CanvasEntry &c : m_projects.takeAt(index).canvases)
        emit canvasRemoved(canvasPath(name, c.name));
    return true;
}

// ---- Moving ----

void Library::moveProject(const QString &projectName, int toIndex)
{
    const int from = projectIndex(projectName);
    if (from < 0)
        return;
    const int to = movedPosition(from, toIndex, m_projects.size());
    if (to == from)
        return;
    m_projects.move(from, to);
    saveIndexAndNotify();
}

QString Library::moveCanvas(const QString &projectName, const QString &canvas, const QString &toProject, int toIndex)
{
    const auto at = findCanvas(projectName, canvas);
    ProjectEntry *target = project(toProject);
    if (!at || !target)
        return {};
    if (toProject == projectName) {
        reorderCanvas(m_projects[at->projectIndex], at->canvasIndex, toIndex);
        return canvas;
    }
    return moveCanvasToProject(m_projects[at->projectIndex], at->canvasIndex, *target, toIndex);
}

void Library::reorderCanvas(ProjectEntry &project, int from, int toIndex)
{
    const int to = movedPosition(from, toIndex, project.canvases.size());
    if (to == from)
        return;
    project.canvases.move(from, to);
    saveIndexAndNotify();
}

// Renamed if its name is taken there ("Auth 2"). Returns its name afterwards, or "".
QString Library::moveCanvasToProject(ProjectEntry &source, int from, ProjectEntry &target, int toIndex)
{
    const QString canvas = source.canvases[from].name;
    if (source.canvases.size() == 1) {
        emit errorOccurred(tr("A project needs at least one canvas, so “%1” has to stay in “%2”.")
                               .arg(canvas, source.name));
        return {};
    }
    emit aboutToChangeFiles();
    const QString name = uniqueName(canvas, canvasNames(target.name));
    const QString oldPath = canvasPath(source.name, canvas);
    const QString newPath = canvasPath(target.name, name);
    if (!renamePath(oldPath, newPath, tr("Could not move “%1” to “%2”.").arg(canvas, target.name)))
        return {};
    CanvasEntry entry = source.canvases.takeAt(from);
    entry.name = name;
    target.canvases.insert(qBound(0, toIndex, target.canvases.size()), entry);
    target.expanded = true;
    if (source.lastCanvas == canvas)
        source.lastCanvas.clear();   // lastCanvas() falls back to its first Canvas
    emit canvasMoved(oldPath, newPath);
    if (m_currentProject == source.name && m_currentCanvas == canvas)
        setCurrent(target.name, name);   // also the target's lastCanvas
    saveIndexAndNotify();
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

std::optional<Viewport> Library::view(const QString &projectName, const QString &canvas) const
{
    const auto at = findCanvas(projectName, canvas);
    if (!at)
        return std::nullopt;
    return m_projects[at->projectIndex].canvases[at->canvasIndex].view;
}

// Not written to disk right away: it changes on every pan step. The index is saved with the
// next change, and when the Library is closed.
void Library::setView(const QString &projectName, const QString &canvas, const Viewport &view)
{
    if (const auto at = findCanvas(projectName, canvas))
        m_projects[at->projectIndex].canvases[at->canvasIndex].view = view;
}

QVariant Library::currentView() const
{
    const std::optional<Viewport> saved = view(m_currentProject, m_currentCanvas);
    return saved ? QVariant::fromValue(*saved) : QVariant();
}

void Library::rememberCurrentView(QPointF center, qreal zoom)
{
    setView(m_currentProject, m_currentCanvas, {center, zoom});
    m_viewSaveTimer.start();
}
