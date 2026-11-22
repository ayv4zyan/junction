#pragma once

// The Library: every Project the user has, kept in one folder the app owns.
//
//   <root>/
//     .junction            the index: order of Projects and Canvases, views, what was open
//     WebApp/              a Project is a folder...
//       Auth.canvas        ...and each Canvas is a JSON Canvas file inside it
//       Overview.canvas
//
// Projects and Canvases are addressed by name (folder name, file name without ".canvas"),
// which is unique within its parent, ignoring case. Every change goes through this class, so
// the index always matches the folders. Folders changed behind the app's back are picked up by
// load(): unlisted files go to the end, listed ones that are gone are skipped.

#include <QJsonObject>
#include <QObject>
#include <QPointF>
#include <QTimer>
#include <QUrl>
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
    QVector<CanvasEntry> canvases;   // never empty
    bool expanded = true;            // shown open in the sidebar
    QString lastCanvas;              // the Canvas last open in this Project
};

// QML calls the Q_INVOKABLE methods directly (the sidebar's list itself is LibraryModel).
class Library : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QString currentProject READ currentProject NOTIFY currentChanged)
    Q_PROPERTY(QString currentCanvas READ currentCanvas NOTIFY currentChanged)
    Q_PROPERTY(QStringList projectNames READ projectNames NOTIFY projectsChanged)
    Q_PROPERTY(QUrl folder READ folder CONSTANT)

public:
    explicit Library(const QString &root, QObject *parent = nullptr);
    ~Library() override;   // saves the index

    // Reads the index and the folders. Afterwards there is always at least one Project, each
    // with at least one Canvas, and one of them is current.
    Q_INVOKABLE void load();
    void saveIndex();

    QString root() const { return m_root; }
    QUrl folder() const { return QUrl::fromLocalFile(m_root); }
    QStringList projectNames() const;
    Q_INVOKABLE int canvasCount(const QString &project) const;
    const QVector<ProjectEntry> &projects() const { return m_projects; }
    int projectIndex(const QString &project) const;
    int canvasIndex(const QString &project, const QString &canvas) const;
    QString projectPath(const QString &project) const;
    QString canvasPath(const QString &project, const QString &canvas) const;

    // The Canvas open in the editor.
    QString currentProject() const { return m_currentProject; }
    QString currentCanvas() const { return m_currentCanvas; }
    QString currentPath() const { return canvasPath(m_currentProject, m_currentCanvas); }
    bool open(const QString &project, const QString &canvas);
    QString lastCanvas(const QString &project) const;   // or its first, if that one is gone

    // Why a name can't be used, or "" when it can. `except` is the item being renamed.
    Q_INVOKABLE QString projectNameProblem(const QString &name, const QString &except = {}) const;
    Q_INVOKABLE QString canvasNameProblem(const QString &project, const QString &name, const QString &except = {}) const;

    // Each returns the new item's name ("" on failure) and opens it.
    QString createProject();
    QString createCanvas(const QString &project);
    Q_INVOKABLE QString duplicateProject(const QString &project);
    Q_INVOKABLE QString duplicateCanvas(const QString &project, const QString &canvas);
    QString importCanvas(const QString &filePath, const QString &project);

    Q_INVOKABLE bool renameProject(const QString &project, const QString &newName);
    Q_INVOKABLE bool renameCanvas(const QString &project, const QString &canvas, const QString &newName);

    // Deleting moves the files to the system trash. The last Canvas of a Project can't be
    // deleted; deleting the last Project leaves a new empty one.
    Q_INVOKABLE bool deleteProject(const QString &project);
    Q_INVOKABLE bool deleteCanvas(const QString &project, const QString &canvas);

    // `toIndex` is a position in the list as it is now, before the item is taken out.
    Q_INVOKABLE void moveProject(const QString &project, int toIndex);
    // Moving into another Project renames the Canvas if the name is taken there ("Auth 2").
    // Returns the Canvas's name afterwards, or "" if it couldn't move.
    Q_INVOKABLE QString moveCanvas(const QString &project, const QString &canvas, const QString &toProject, int toIndex);

    Q_INVOKABLE void setExpanded(const QString &project, bool expanded);
    std::optional<Viewport> view(const QString &project, const QString &canvas) const;
    void setView(const QString &project, const QString &canvas, const Viewport &view);
    // The same for the open Canvas, for QML: currentView() is a Viewport, or null when it was
    // never panned or zoomed. A remembered view is written to the index a moment later.
    Q_INVOKABLE QVariant currentView() const;
    Q_INVOKABLE void rememberCurrentView(QPointF center, qreal zoom);

    static QString nameProblem(const QString &name);   // characters and length only
    static QString uniqueName(const QString &base, const QStringList &taken);

signals:
    void loaded();          // load() is done; currentPath() is empty if the folder can't be written
    void aboutToChange();   // files are about to change or another Canvas to open: save now
    void projectsChanged(); // Projects or Canvases were added, removed, renamed or reordered
    void expandedChanged(const QString &project);
    void currentChanged();  // currentPath() changed: another Canvas is open, or it was renamed or moved
    void canvasMoved(const QString &oldPath, const QString &newPath);   // renamed or moved
    void canvasRemoved(const QString &path);
    void errorOccurred(const QString &message);

private:
    ProjectEntry *project(const QString &name);
    QStringList canvasNames(const QString &project) const;
    QJsonObject readIndex();
    void openRemembered(const QString &project, const QString &canvas);
    QStringList canvasFiles(const QString &project) const;   // names of the .canvas files in its folder
    void setCurrent(const QString &project, const QString &canvas);
    bool createEmptyCanvasFile(const QString &path);
    bool renamePath(const QString &from, const QString &to);
    // Every change ends here: saves the index, emits projectsChanged(), then currentChanged() if
    // currentPath() is no longer the one last announced.
    void saveAndAnnounce();
    void announceCurrent();
    QString addProject(int at);

    QString m_root;
    QVector<ProjectEntry> m_projects;
    QString m_currentProject;
    QString m_currentCanvas;
    QString m_announcedPath;   // currentPath() as of the last currentChanged()
    QTimer m_viewSaveTimer;    // saves the index once the view stops changing
};
