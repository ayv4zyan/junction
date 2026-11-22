#include "Exporter.h"

#include "CanvasPainter.h"

Exporter::Exporter(const Document *document, QObject *parent)
    : QObject(parent), m_document(document)
{
}

QUrl Exporter::exportCanvasFile(const QUrl &url)
{
    QString path = url.toLocalFile();
    if (!path.endsWith(".canvas", Qt::CaseInsensitive))
        path += ".canvas";
    QString error;
    if (!m_document->save(path, &error)) {
        emit errorOccurred(tr("Could not save %1: %2").arg(path, error));
        return {};
    }
    return QUrl::fromLocalFile(path);
}

QUrl Exporter::exportImage(const QUrl &url, const QVariantMap &colors, const QStringList &ids)
{
    const std::optional<Palette> palette = Palette::fromVariantMap(colors);
    if (!palette) {
        emit errorOccurred(tr("The colors to export with are incomplete."));
        return {};
    }
    QString error;
    if (!CanvasPainter::exportImage(*m_document, m_document->nodesWithContents(ids), url.toLocalFile(), *palette, &error)) {
        emit errorOccurred(error);
        return {};
    }
    return url;
}
