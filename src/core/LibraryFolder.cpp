#include "LibraryFolder.h"

#include "Document.h"
#include "LibraryNames.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

namespace {

const QString IndexFile = QStringLiteral(".junction");
const QString CanvasExtension = QStringLiteral(".canvas");
constexpr auto SortOrder = QDir::Name | QDir::IgnoreCase | QDir::LocaleAware;

} // namespace

LibraryFolder::LibraryFolder(const QString &root)
    : m_root(root)
{
}

QString LibraryFolder::projectPath(const QString &project) const
{
    return QDir(m_root).filePath(project);
}

QString LibraryFolder::canvasPath(const QString &project, const QString &canvas) const
{
    if (project.isEmpty() || canvas.isEmpty())
        return {};
    return QDir(projectPath(project)).filePath(canvas + CanvasExtension);
}

bool LibraryFolder::create() const
{
    return QDir().mkpath(m_root);
}

QStringList LibraryFolder::projectFolders() const
{
    return QDir(m_root).entryList(QDir::Dirs | QDir::NoDotAndDotDot, SortOrder);
}

QStringList LibraryFolder::canvasFiles(const QString &project) const
{
    QStringList names;
    for (const QString &file : QDir(projectPath(project)).entryList({"*" + CanvasExtension}, QDir::Files, SortOrder))
        names.append(file.chopped(CanvasExtension.size()));
    return names;
}

std::optional<LibraryIndex> LibraryFolder::readIndex() const
{
    QFile file(QDir(m_root).filePath(IndexFile));
    if (!file.exists())
        return LibraryIndex{};
    const QJsonDocument json = file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()) : QJsonDocument();
    if (!json.isObject())
        return std::nullopt;
    return LibraryIndex::fromJson(json.object());
}

QString LibraryFolder::keepBrokenIndex() const
{
    const QString path = QDir(m_root).filePath(IndexFile);
    const QString broken = path + QStringLiteral(".broken");
    QFile::remove(broken);   // an older broken index, if any
    return QFile::copy(path, broken) ? broken : QString();
}

bool LibraryFolder::writeIndex(const LibraryIndex &index) const
{
    QSaveFile file(QDir(m_root).filePath(IndexFile));
    return file.open(QIODevice::WriteOnly)
           && file.write(QJsonDocument(index.toJson()).toJson(QJsonDocument::Indented)) >= 0 && file.commit();
}

bool LibraryFolder::createProject(const QString &project) const
{
    return QDir(m_root).mkpath(project);
}

bool LibraryFolder::createEmptyCanvas(const QString &project, const QString &canvas, QString *error) const
{
    if (!createProject(project)) {
        *error = QCoreApplication::translate("Library", "its folder could not be created");
        return false;
    }
    return Document().save(canvasPath(project, canvas), error);
}

bool LibraryFolder::copyIn(const QString &file, const QString &project, const QString &canvas) const
{
    return QFile::copy(file, canvasPath(project, canvas));
}

bool LibraryFolder::copyProject(const QString &from, const QString &to, const QStringList &canvases) const
{
    if (!createProject(to))
        return false;
    for (const QString &canvas : canvases) {
        if (!copyIn(canvasPath(from, canvas), to, canvas))
            return false;
    }
    return true;
}

bool LibraryFolder::move(const QString &from, const QString &to, QString *stranded) const
{
    if (!LibraryNames::same(from, to))
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

bool LibraryFolder::trash(const QString &path) const
{
    return !QFileInfo::exists(path) || QFile::moveToTrash(path);
}

bool LibraryFolder::removeProject(const QString &project) const
{
    return QDir(projectPath(project)).removeRecursively();
}
