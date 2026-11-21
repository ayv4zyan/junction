#pragma once

// QPainter drawing of a Canvas. The live view uses it for the grid and Connections;
// export uses it for everything.

#include "Document.h"
#include "Geometry.h"

#include <QColor>
#include <QVariantMap>

class QPainter;

// Colors come from the QML Theme (the single place colors are defined) as a QVariantMap.
struct Palette
{
    QColor background{"#1e1e1e"};
    QColor grid{"#3a3a3a"};
    QColor cardFill{"#262626"};
    QColor cardBorder{"#555555"};
    QColor text{"#dcddde"};
    QColor mutedText{"#8a8a8a"};
    QColor line{"#8a8a8a"};
    QColor selection{"#7f6df2"};
    QVector<QColor> presets;   // JSON Canvas colors "1".."6"

    QColor resolve(const QString &color, const QColor &fallback) const;
    static Palette fromVariantMap(const QVariantMap &map);
};

namespace CanvasPainter {

// `view` maps world coordinates to the screen; the grid is drawn in screen space.
void drawGrid(QPainter &painter, const QTransform &view, const QSizeF &screenSize, const Palette &palette);

// These draw in world coordinates (set the painter's transform first).
void drawConnection(QPainter &painter, const Geometry::Route &route, const Connection &connection,
                    const Palette &palette, bool selected);
void drawNode(QPainter &painter, const Node &node, const Palette &palette);

// Writes PNG or SVG depending on the file extension. Empty `onlyIds` means the whole Canvas.
bool exportCanvas(const Document &document, const QStringList &onlyIds, const QString &path,
                  const Palette &palette, QString *error);

} // namespace CanvasPainter
