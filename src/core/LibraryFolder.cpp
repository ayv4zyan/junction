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

LibraryFolder::IndexRead LibraryFolder::readIndex() const
{
    const QString path = QDir(m_root).filePath(IndexFile);
    QFile file(path);
    if (!file.exists())
        return {};
    const QJsonDocument json = file.open(QIODevice::ReadOnly) ? QJsonDocument::fromJson(file.readAll()) : QJsonDocument();
    if (json.isObject())
        return {LibraryIndex::fromJson(json.object())};
    file.close();
    const QString broken = path + QStringLiteral(".broken");
    QFile::remove(broken);   // an older broken index, if any
    return {{}, true, QFile::copy(path, broken) ? broken : QString()};
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

bool LibraryFolder::copyFile(const QString &from, const QString &to) const
{
    return QFile::copy(from, to);
}

bool LibraryFolder::copyProject(const QString &from, const QString &to, const QStringList &canvases) const
{
    if (!createProject(to))
        return false;
    for (const QString &canvas : canvases) {
        if (!QFile::copy(canvasPath(from, canvas), canvasPath(to, canvas)))
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

void LibraryFolder::removeProject(const QString &project) const
{
    QDir(projectPath(project)).removeRecursively();
}
