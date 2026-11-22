#pragma once

// Two full-window layers drawn with QPainter (via CanvasPainter) in screen space, so lines
// and dots stay crisp at every zoom level. Cards and Groups are normal QML items instead.

#include "CanvasPainter.h"
#include "Editor.h"

#include <QQuickPaintedItem>

class CanvasLayer : public QQuickPaintedItem
{
    Q_OBJECT
    QML_ANONYMOUS
    Q_PROPERTY(qreal zoom MEMBER m_zoom NOTIFY viewChanged)
    Q_PROPERTY(QPointF pan MEMBER m_pan NOTIFY viewChanged)
    // From Colors.forPainter (src/design); read into a Palette once, when set.
    Q_PROPERTY(QVariantMap colors WRITE setColors)

public:
    explicit CanvasLayer(QQuickItem *parent = nullptr);
    void setColors(const QVariantMap &colors);

signals:
    void viewChanged();

protected:
    QTransform worldToScreen() const { return QTransform(m_zoom, 0, 0, m_zoom, m_pan.x(), m_pan.y()); }
    const std::optional<Palette> &palette() const { return m_palette; }   // nothing until colors are set

private:
    qreal m_zoom = 1;
    QPointF m_pan;
    std::optional<Palette> m_palette;
};

class GridLayer : public CanvasLayer
{
    Q_OBJECT
    QML_ELEMENT

public:
    using CanvasLayer::CanvasLayer;
    void paint(QPainter *painter) override;
};

class ConnectionLayer : public CanvasLayer
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(Editor *editor READ editor WRITE setEditor NOTIFY editorChanged)
    // The Connection whose label is being edited: its label is not drawn, LabelEditor shows it.
    Q_PROPERTY(QString editedLabelConnection MEMBER m_editedLabelConnection NOTIFY interactionChanged)
    // The new Connection being dragged out of a Handle of the Card `connectFrom` (none while
    // that is ""). It ends on `dropTarget` (at `dropSide`) when the pointer is over a Card,
    // otherwise at `connectPointer` (world coordinates).
    Q_PROPERTY(QString connectFrom MEMBER m_connectFrom NOTIFY interactionChanged)
    Q_PROPERTY(QPointF connectPointer MEMBER m_connectPointer NOTIFY interactionChanged)
    Q_PROPERTY(QString dropTarget MEMBER m_dropTarget NOTIFY interactionChanged)
    Q_PROPERTY(CanvasEnums::Side dropSide MEMBER m_dropSide NOTIFY interactionChanged)

public:
    explicit ConnectionLayer(QQuickItem *parent = nullptr);
    Editor *editor() const { return m_editor; }
    void setEditor(Editor *editor);
    void paint(QPainter *painter) override;

signals:
    void editorChanged();
    void interactionChanged();   // a label is edited or a new Connection dragged out

private:
    Editor *m_editor = nullptr;
    QString m_editedLabelConnection;
    QString m_connectFrom;
    QPointF m_connectPointer;
    QString m_dropTarget;
    Side m_dropSide = Side::Floating;
};
