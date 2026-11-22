#include "CanvasLayers.h"

#include <QPainter>

CanvasLayer::CanvasLayer(QQuickItem *parent)
    : QQuickPaintedItem(parent)
{
    setAntialiasing(true);
    connect(this, &CanvasLayer::viewChanged, this, [this] { update(); });
}

void GridLayer::paint(QPainter *painter)
{
    CanvasPainter::drawGrid(*painter, view(), size(), colors());
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
    connect(editor, &Editor::previewChanged, this, repaint);
}

void ConnectionLayer::paint(QPainter *painter)
{
    if (!m_editor)
        return;
    const Document &document = m_editor->document();
    const Palette palette = colors();
    painter->setRenderHint(QPainter::Antialiasing);
    painter->setTransform(view());

    for (const Connection &c : document.connections()) {
        if (const auto route = document.route(c)) {
            CanvasPainter::drawConnection(*painter, *route, c, palette, m_editor->selection().contains(c.id),
                                          c.id != m_hiddenLabel,
                                          CanvasPainter::colorUnder(route->labelPosition, document.nodes(), palette));
        }
    }

    // The Connection being dragged out of a Handle. Without a target it ends at the pointer.
    const auto &preview = m_editor->connectionPreview();
    const Node *from = preview ? document.node(preview->fromNode) : nullptr;
    if (from) {
        const Node *target = document.node(preview->targetNode);
        const QRectF to = target ? target->rect : QRectF(preview->pointer, QSizeF(0, 0));
        Connection c;
        c.toSide = preview->targetSide;
        CanvasPainter::drawConnection(*painter, Geometry::route(from->rect, to, c), c, palette, true);
    }
}
