#include "CanvasLayers.h"

#include <QPainter>

CanvasLayer::CanvasLayer(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    connect(this, &CanvasLayer::viewChanged, this, [this] { update(); });
}

void CanvasLayer::setColors(const QVariantMap &colors)
{
    m_palette = Palette::fromVariantMap(colors);
    if (!m_palette)
        qWarning("CanvasLayer: Colors.forPainter is missing a color; nothing is painted");
    update();
}

void GridLayer::paint(QPainter *painter)
{
    if (palette())
        CanvasPainter::drawGrid(*painter, worldToScreen(), size(), *palette());
}

ConnectionLayer::ConnectionLayer(QQuickItem *parent)
    : CanvasLayer(parent)
{
    connect(this, &ConnectionLayer::interactionChanged, this, [this] { update(); });
}

void ConnectionLayer::setEditor(Editor *editor)
{
    if (editor == m_editor)
        return;
    if (m_editor) {
        disconnect(m_editor, nullptr, this, nullptr);
        disconnect(&m_editor->document(), nullptr, this, nullptr);
    }
    m_editor = editor;
    emit editorChanged();
    update();
    if (!editor)
        return;

    // Repaint whenever anything that affects a line changes.
    auto repaint = [this] { update(); };
    const Document *document = &editor->document();
    connect(document, &Document::nodeChanged, this, repaint);
    connect(document, &Document::nodeRemoved, this, repaint);
    connect(document, &Document::connectionsChanged, this, repaint);
    connect(document, &Document::reset, this, repaint);
    connect(editor, &Editor::selectionChanged, this, repaint);
}

void ConnectionLayer::paint(QPainter *painter)
{
    if (!m_editor || !this->palette())
        return;
    const Palette &palette = *this->palette();
    const Document &document = m_editor->document();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setTransform(worldToScreen());

    for (const Connection &c : document.connections()) {
        if (const auto route = document.route(c)) {
            CanvasPainter::drawConnection(*painter, *route, c, palette, m_editor->selection().contains(c.id),
                                          c.id != m_editedLabelConnection,
                                          CanvasPainter::colorUnder(route->labelPosition, document.nodes(), palette));
        }
    }

    // The new Connection being dragged out of a Handle.
    if (const Node *from = document.node(m_connectFrom)) {
        const Node *target = document.node(m_dropTarget);
        const QRectF to = target ? target->rect : QRectF(m_connectPointer, QSizeF(0, 0));
        Connection c;
        c.toSide = m_dropSide;
        CanvasPainter::drawConnection(*painter, Geometry::route(from->rect, to, c), c, palette, true);
    }
}
