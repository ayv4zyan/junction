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
    Q_PROPERTY(QVariantMap colors MEMBER m_colors NOTIFY viewChanged)

public:
    explicit CanvasLayer(QQuickItem *parent = nullptr);

signals:
    void viewChanged();

protected:
    QTransform view() const { return QTransform(m_zoom, 0, 0, m_zoom, m_pan.x(), m_pan.y()); }
    Palette colors() const { return Palette::fromVariantMap(m_colors); }

private:
    qreal m_zoom = 1;
    QPointF m_pan;
    QVariantMap m_colors;   // from Colors.forPainter (src/design)
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
    // The Connection whose label is being edited: its label is not drawn, the editor shows it.
    Q_PROPERTY(QString hiddenLabel MEMBER m_hiddenLabel NOTIFY viewChanged)

public:
    using CanvasLayer::CanvasLayer;
    Editor *editor() const { return m_editor; }
    void setEditor(Editor *editor);
    void paint(QPainter *painter) override;

signals:
    void editorChanged();

private:
    Editor *m_editor = nullptr;
    QString m_hiddenLabel;
};
