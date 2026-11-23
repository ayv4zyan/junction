#include "Library.h"

#include "Document.h"
#include "LibraryNames.h"

#include <QFileInfo>

namespace {

const QString UntitledName = QStringLiteral("Untitled");   // what a new Project or Canvas is called

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
	: QObject(parent), m_folder(root)
{
	m_viewSaveTimer.setSingleShot(true);
	m_viewSaveTimer.setInterval(1000);
	connect(&m_viewSaveTimer, &QTimer::timeout, this, &Library::saveIndex);
}

// Saves the index one last time. Nobody is listening for errorOccurred any more, so a failure
// goes to the log.
Library::~Library()
{
	if (!m_index.projects.isEmpty() && !m_folder.writeIndex(m_index))
		qWarning("Junction: the library index could not be saved");
}

// ---- Loading and saving ----

void Library::load()
{
	if (const std::optional<LibraryIndex> index = loadProjects()) {
		openWhatWasOpenLastTime(index->currentProject, index->currentCanvas);
		saveIndexAndNotify();
	}
	emit loaded();
}

// Reads the index and the folders into m_index.projects; what is really on disk wins. Returns the
// index for what was open last time, or nothing when the folder can't be written
// (errorOccurred has said so).
std::optional<LibraryIndex> Library::loadProjects()
{
	if (!m_folder.create()) {
		emit errorOccurred(tr("Could not create the library folder %1.").arg(m_folder.root()));
		return std::nullopt;
	}
	const LibraryIndex index = readIndex();
	m_index.projects.clear();
	const QStringList folders = m_folder.projectFolders();
	const auto newProject = [](const QString &name) { return ProjectEntry{name, {}, true, {}}; };
	const auto newCanvas = [](const QString &name) { return CanvasEntry{name, std::nullopt}; };
	for (ProjectEntry p : reconciled(index.projects, folders, newProject)) {
		p.canvases = reconciled(p.canvases, m_folder.canvasFiles(p.name), newCanvas);
		// A Project always has a Canvas to show; an empty folder gets a new one.
		if (p.canvases.isEmpty()) {
			if (!createEmptyCanvas(p.name, UntitledName))
				continue;
			p.canvases.append(newCanvas(UntitledName));
		}
		m_index.projects.append(p);
	}
	if (m_index.projects.isEmpty() && !addProject(0))
		return std::nullopt;
	return index;
}

// The index: order, views, expanded state, the open Canvas. When it can't be read (load() is
// about to write a new one), the user is told that the order and views were reset.
LibraryIndex Library::readIndex()
{
	if (std::optional<LibraryIndex> index = m_folder.readIndex())
		return *index;
	QString message = tr("The library index couldn't be read, so the order of projects and canvases was reset.");
	if (const QString keptAs = m_folder.keepBrokenIndex(); !keptAs.isEmpty())
		message += ' ' + tr("The old index was kept as %1.").arg(keptAs);
	emit errorOccurred(message);
	return {};
}

// The Canvas open last time; if it is gone, the first of its Project, or else the very first.
void Library::openWhatWasOpenLastTime(const QString &project, const QString &canvas)
{
	if (m_index.canvasPosition(project, canvas))
		setCurrent(project, canvas);
	else if (m_index.project(project))
		setCurrent(project, m_index.lastOpenedCanvas(project));
	else
		setCurrent(m_index.projects.first().name, m_index.projects.first().firstCanvas());
}

void Library::saveIndex()
{
	if (!m_folder.writeIndex(m_index))
		emit errorOccurred(tr("Could not save the library index in %1.").arg(m_folder.root()));
}

void Library::saveIndexAndNotify()
{
	saveIndex();
	emit projectsChanged();
	notifyIfCurrentPathChanged();
}

void Library::notifyIfCurrentPathChanged()
{
	if (currentPath() == m_lastAnnouncedPath)
		return;
	m_lastAnnouncedPath = currentPath();
	emit currentChanged();
}

// ---- Looking things up ----

QString Library::canvasPath(const QString &project, const QString &canvas) const
{
	return m_folder.canvasPath(project, canvas);
}

int Library::canvasCount(const QString &project) const
{
	const ProjectEntry *p = m_index.project(project);
	return p ? int(p->canvases.size()) : 0;
}

// ---- Names ----

QString Library::projectNameProblem(const QString &name, const QString &except) const
{
	return LibraryNames::problemAmong(name.trimmed(), m_index.projectNames(), except,
									  tr("There is already a project named “%1”."));
}

QString Library::canvasNameProblem(const QString &project, const QString &name, const QString &except) const
{
	return LibraryNames::problemAmong(name.trimmed(), m_index.canvasNames(project), except,
									  tr("“%1” already has a canvas named “%2”.").arg(project));
}

// ---- The open Canvas ----

void Library::setCurrent(const QString &project, const QString &canvas)
{
	if (project == m_index.currentProject && canvas == m_index.currentCanvas)
		return;
	m_index.currentProject = project;
	m_index.currentCanvas = canvas;
	if (ProjectEntry *p = m_index.project(project))
		p->lastOpened = canvas;
}

// Only the open Canvas changes here, not the list: no projectsChanged().
bool Library::openProject(const QString &projectName)
{
	return open(projectName, m_index.lastOpenedCanvas(projectName));
}

bool Library::open(const QString &projectName, const QString &canvas)
{
	if (!m_index.canvasPosition(projectName, canvas))
		return false;
	if (projectName == m_index.currentProject && canvas == m_index.currentCanvas) {
		setExpanded(projectName, true);
		return true;
	}
	emit openCanvasMustBeSaved();
	setCurrent(projectName, canvas);
	ProjectEntry *p = m_index.project(projectName);
	const bool expand = !p->expanded;   // so the open Canvas can be seen in the sidebar
	p->expanded = true;
	saveIndex();
	if (expand)
		emit expandedChanged(projectName);
	notifyIfCurrentPathChanged();
	return true;
}

void Library::openAdjacent(int step)
{
	const auto at = m_index.canvasPosition(m_index.currentProject, m_index.currentCanvas);
	if (!at)
		return;
	const QVector<CanvasEntry> &canvases = m_index.projects[at->project].canvases;
	const int next = at->canvas + step;
	if (next >= 0 && next < canvases.size())
		open(m_index.currentProject, canvases[next].name);
}

// ---- Creating ----

bool Library::createEmptyCanvas(const QString &project, const QString &canvas)
{
	QString error;
	if (m_folder.createEmptyCanvas(project, canvas, &error))
		return true;
	emit errorOccurred(tr("Could not create %1: %2").arg(canvasPath(project, canvas), error));
	return false;
}

// After a create or copy failed half way: the folder it left goes, or the user hears it is there.
void Library::removeUnfinishedProject(const QString &project)
{
	if (!m_folder.removeProject(project))
		emit errorOccurred(tr("The unfinished folder %1 could not be removed.").arg(m_folder.projectPath(project)));
}

// A new Project folder with one empty Canvas, inserted at `at`.
std::optional<QString> Library::addProject(int at)
{
	const QString name = LibraryNames::unique(UntitledName, m_index.projectNames());
	if (!m_folder.createProject(name)) {
		emit errorOccurred(tr("Could not create the folder %1.").arg(m_folder.projectPath(name)));
		return std::nullopt;
	}
	if (!createEmptyCanvas(name, UntitledName)) {
		removeUnfinishedProject(name);
		return std::nullopt;
	}
	m_index.projects.insert(at, {name, {{UntitledName, std::nullopt}}, true, {}});
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

std::optional<QString> Library::createProject()
{
	emit openCanvasMustBeSaved();
	const std::optional<QString> name = addProject(m_index.projects.size());
	if (!name)
		return std::nullopt;
	setCurrent(*name, UntitledName);
	saveIndexAndNotify();
	emit createdEmpty(*name, {});
	return name;
}

std::optional<QString> Library::createCanvas(const QString &projectName)
{
	ProjectEntry *p = m_index.project(projectName);
	if (!p)
		return std::nullopt;
	emit openCanvasMustBeSaved();
	const QString name = LibraryNames::unique(UntitledName, m_index.canvasNames(projectName));
	if (!createEmptyCanvas(projectName, name))
		return std::nullopt;
	addCanvas(p, p->canvases.size(), {name, std::nullopt});
	emit createdEmpty(projectName, name);
	return name;
}

std::optional<QString> Library::duplicateCanvas(const QString &projectName, const QString &canvas)
{
	const auto at = m_index.canvasPosition(projectName, canvas);
	if (!at)
		return std::nullopt;
	emit openCanvasMustBeSaved();   // the open Canvas is saved first, so the copy has its latest state
	const QString name = LibraryNames::unique(tr("%1 copy").arg(canvas), m_index.canvasNames(projectName));
	if (!m_folder.copyIn(canvasPath(projectName, canvas), projectName, name)) {
		emit errorOccurred(tr("Could not copy “%1”.").arg(canvas));
		return std::nullopt;
	}
	ProjectEntry &p = m_index.projects[at->project];
	CanvasEntry copy = p.canvases[at->canvas];
	copy.name = name;
	return addCanvas(&p, at->canvas + 1, copy);
}

std::optional<QString> Library::duplicateProject(const QString &projectName)
{
	const std::optional<int> i = m_index.projectPosition(projectName);
	if (!i)
		return std::nullopt;
	emit openCanvasMustBeSaved();
	const QString name = LibraryNames::unique(tr("%1 copy").arg(projectName), m_index.projectNames());
	ProjectEntry copy = m_index.projects[*i];
	copy.name = name;
	if (!m_folder.copyProject(projectName, name, m_index.canvasNames(projectName))) {
		removeUnfinishedProject(name);
		emit errorOccurred(tr("Could not copy “%1”.").arg(projectName));
		return std::nullopt;
	}
	m_index.projects.insert(*i + 1, copy);
	setCurrent(name, copy.firstCanvas());
	saveIndexAndNotify();
	return name;
}

std::optional<QString> Library::importCanvas(const QString &filePath, const QString &projectName)
{
	ProjectEntry *p = m_index.project(projectName);
	if (!p)
		return std::nullopt;
	Document probe;   // only files that open as a Canvas are copied in
	if (QString error; !probe.load(filePath, &error)) {
		emit errorOccurred(tr("%1 can't be imported: %2").arg(QFileInfo(filePath).fileName(), error));
		return std::nullopt;
	}

	emit openCanvasMustBeSaved();
	QString base = QFileInfo(filePath).completeBaseName().trimmed();
	if (!LibraryNames::problem(base).isEmpty())
		base = tr("Imported canvas");
	const QString name = LibraryNames::unique(base, m_index.canvasNames(projectName));
	if (!m_folder.copyIn(filePath, projectName, name)) {
		emit errorOccurred(tr("Could not copy %1 into the project.").arg(filePath));
		return std::nullopt;
	}
	return addCanvas(p, p->canvases.size(), {name, std::nullopt});
}

void Library::importCanvases(const QList<QUrl> &files)
{
	for (const QUrl &file : files) {
		if (file.isLocalFile())
			importCanvas(file.toLocalFile(), m_index.currentProject);
	}
}

// ---- Renaming ----

// What renaming a Project and a Canvas share: a name with a `problem` is refused (and said so);
// otherwise the open Canvas is saved and the folder or file renamed.
bool Library::renameIfNameIsFree(const QString &problem, const QString &from, const QString &to, const QString &failure)
{
	if (!problem.isEmpty()) {
		emit errorOccurred(problem);
		return false;
	}
	emit openCanvasMustBeSaved();
	return renameFile(from, to, failure);
}

// Moves the folder or file (see LibraryFolder::move()); on failure says `failure`, and where the
// file ended up if it couldn't be put back.
bool Library::renameFile(const QString &from, const QString &to, const QString &failure)
{
	QString stranded;
	if (m_folder.move(from, to, &stranded))
		return true;
	if (stranded.isEmpty())
		emit errorOccurred(failure);
	else
		emit errorOccurred(failure + ' ' + tr("It could not be renamed back either, and is now called %1.").arg(stranded));
	return false;
}

bool Library::renameProject(const QString &projectName, const QString &newName)
{
	const QString name = newName.trimmed();
	ProjectEntry *p = m_index.project(projectName);
	if (!p)
		return false;
	if (name == projectName)
		return true;   // nothing to do
	if (!renameIfNameIsFree(projectNameProblem(name, projectName), m_folder.projectPath(projectName),
							m_folder.projectPath(name),
					  tr("Could not rename “%1” to “%2”.").arg(projectName, name)))
		return false;
	p->name = name;
	for (const CanvasEntry &c : p->canvases)
		emit canvasMoved(canvasPath(projectName, c.name), canvasPath(name, c.name));
	if (m_index.currentProject == projectName)
		m_index.currentProject = name;
	saveIndexAndNotify();
	return true;
}

bool Library::renameCanvas(const QString &projectName, const QString &canvas, const QString &newName)
{
	const QString name = newName.trimmed();
	const auto at = m_index.canvasPosition(projectName, canvas);
	if (!at)
		return false;
	if (name == canvas)
		return true;   // nothing to do
	const QString from = canvasPath(projectName, canvas);
	const QString to = canvasPath(projectName, name);
	if (!renameIfNameIsFree(canvasNameProblem(projectName, name, canvas), from, to,
					  tr("Could not rename “%1” to “%2”.").arg(canvas, name)))
		return false;
	ProjectEntry &p = m_index.projects[at->project];
	p.canvases[at->canvas].name = name;
	if (p.lastOpened == canvas)
		p.lastOpened = name;
	emit canvasMoved(from, to);
	if (m_index.currentProject == projectName && m_index.currentCanvas == canvas)
		m_index.currentCanvas = name;
	saveIndexAndNotify();
	return true;
}

// ---- Deleting ----

bool Library::deleteCanvas(const QString &projectName, const QString &canvas)
{
	const auto at = m_index.canvasPosition(projectName, canvas);
	if (!at)
		return false;
	ProjectEntry *p = &m_index.projects[at->project];
	const int i = at->canvas;
	if (p->canvases.size() == 1) {
		emit errorOccurred(tr("A project needs at least one canvas. To remove this one, delete the project."));
		return false;
	}
	emit openCanvasMustBeSaved();
	const QString path = canvasPath(projectName, canvas);
	if (!m_folder.trash(path)) {
		emit errorOccurred(tr("Could not move “%1” to the trash.").arg(canvas));
		return false;
	}
	p->canvases.removeAt(i);
	emit canvasRemoved(path);
	// The open Canvas is replaced by the one that took its place, or the one before it.
	if (m_index.currentProject == projectName && m_index.currentCanvas == canvas)
		setCurrent(projectName, p->canvases[qMin(i, p->canvases.size() - 1)].name);
	saveIndexAndNotify();
	return true;
}

bool Library::deleteProject(const QString &projectName)
{
	const std::optional<int> i = m_index.projectPosition(projectName);
	if (!i)
		return false;
	emit openCanvasMustBeSaved();
	if (m_index.projects.size() == 1)
		return replaceLastProject();
	if (!trashProject(*i))
		return false;
	if (m_index.currentProject == projectName) {   // the Project that took its place, or the one before
		const ProjectEntry &next = m_index.projects[qMin(*i, m_index.projects.size() - 1)];
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
	const std::optional<QString> replacement = addProject(1);
	if (!replacement)
		return false;   // errorOccurred has said why
	if (!trashProject(0)) {
		removeUnfinishedProject(*replacement);
		m_index.projects.removeLast();
		return false;
	}
	ProjectEntry &project = m_index.projects.first();
	QString stranded;   // a free name was taken, so not getting "Untitled" back isn't worth a message
	if (project.name != UntitledName
		&& m_folder.move(m_folder.projectPath(project.name), m_folder.projectPath(UntitledName), &stranded))
		project.name = UntitledName;
	setCurrent(project.name, project.firstCanvas());
	saveIndexAndNotify();
	return true;
}

// Moves the Project's folder to the trash and takes it out of the list.
bool Library::trashProject(int index)
{
	const QString name = m_index.projects[index].name;
	if (!m_folder.trash(m_folder.projectPath(name))) {
		emit errorOccurred(tr("Could not move “%1” to the trash.").arg(name));
		return false;
	}
	for (const CanvasEntry &c : m_index.projects.takeAt(index).canvases)
		emit canvasRemoved(canvasPath(name, c.name));
	return true;
}

// ---- Moving ----

void Library::moveProject(const QString &projectName, int toIndex)
{
	const std::optional<int> from = m_index.projectPosition(projectName);
	if (!from)
		return;
	const int to = movedPosition(*from, toIndex, m_index.projects.size());
	if (to == *from)
		return;
	m_index.projects.move(*from, to);
	saveIndexAndNotify();
}

std::optional<QString> Library::moveCanvas(const QString &projectName, const QString &canvas, const QString &toProject, int toIndex)
{
	const auto at = m_index.canvasPosition(projectName, canvas);
	ProjectEntry *target = m_index.project(toProject);
	if (!at || !target)
		return std::nullopt;
	if (toProject == projectName) {
		reorderCanvas(m_index.projects[at->project], at->canvas, toIndex);
		return canvas;
	}
	return moveCanvasToProject(m_index.projects[at->project], at->canvas, *target, toIndex);
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
std::optional<QString> Library::moveCanvasToProject(ProjectEntry &source, int from, ProjectEntry &target, int toIndex)
{
	const QString canvas = source.canvases[from].name;
	if (source.canvases.size() == 1) {
		emit errorOccurred(tr("A project needs at least one canvas, so “%1” has to stay in “%2”.")
							   .arg(canvas, source.name));
		return std::nullopt;
	}
	emit openCanvasMustBeSaved();
	const QString name = LibraryNames::unique(canvas, m_index.canvasNames(target.name));
	const QString oldPath = canvasPath(source.name, canvas);
	const QString newPath = canvasPath(target.name, name);
	if (!renameFile(oldPath, newPath, tr("Could not move “%1” to “%2”.").arg(canvas, target.name)))
		return std::nullopt;
	CanvasEntry entry = source.canvases.takeAt(from);
	entry.name = name;
	target.canvases.insert(qBound(0, toIndex, target.canvases.size()), entry);
	target.expanded = true;
	if (source.lastOpened == canvas)
		source.lastOpened.clear();   // lastOpenedCanvas() then falls back to its first Canvas
	emit canvasMoved(oldPath, newPath);
	if (m_index.currentProject == source.name && m_index.currentCanvas == canvas)
		setCurrent(target.name, name);   // also the target's lastOpened
	saveIndexAndNotify();
	return name;
}

// ---- Sidebar and view state ----

void Library::setExpanded(const QString &projectName, bool expanded)
{
	ProjectEntry *p = m_index.project(projectName);
	if (!p || p->expanded == expanded)
		return;
	p->expanded = expanded;
	saveIndex();
	emit expandedChanged(projectName);
}

std::optional<Viewport> Library::view(const QString &projectName, const QString &canvas) const
{
	const auto at = m_index.canvasPosition(projectName, canvas);
	if (!at)
		return std::nullopt;
	return m_index.projects[at->project].canvases[at->canvas].view;
}

// Not written to disk right away: it changes on every pan step. The index is saved with the
// next change, and when the Library is closed.
void Library::setView(const QString &projectName, const QString &canvas, const Viewport &view)
{
	if (const auto at = m_index.canvasPosition(projectName, canvas))
		m_index.projects[at->project].canvases[at->canvas].view = view;
}

QVariant Library::currentView() const
{
	const std::optional<Viewport> saved = view(m_index.currentProject, m_index.currentCanvas);
	return saved ? QVariant::fromValue(*saved) : QVariant();
}

void Library::rememberCurrentView(QPointF center, qreal zoom)
{
	setView(m_index.currentProject, m_index.currentCanvas, {center, zoom});
	m_viewSaveTimer.start();
}
