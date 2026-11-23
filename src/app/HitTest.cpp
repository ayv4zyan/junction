#include "HitTest.h"

#include "CanvasPainter.h"

HitTest::HitTest(const Document *document, QObject *parent)
	: QObject(parent), m_document(document)
{
}

QString HitTest::cardAt(QPointF point, qreal margin) const
{
	const QVector<Node> &nodes = m_document->nodes();
	for (auto it = nodes.crbegin(); it != nodes.crend(); ++it) {
		if (it->isCard() && it->rect.adjusted(-margin, -margin, margin, margin).contains(point))
			return it->id;
	}
	return {};
}

Side HitTest::handleAt(const QString &nodeId, QPointF point, qreal radius) const
{
	const Node *n = m_document->node(nodeId);
	return n ? Geometry::handleAt(n->rect, point, radius) : Side::Floating;
}

QString HitTest::connectionAt(QPointF point, qreal tolerance) const
{
	const QVector<Connection> &connections = m_document->connections();
	for (auto it = connections.crbegin(); it != connections.crend(); ++it) {
		const auto route = m_document->route(*it);
		// The label counts too: it can stick out far from the line.
		if (route && (Geometry::hits(*route, point, tolerance) || CanvasPainter::labelRect(*route, it->label).contains(point)))
			return it->id;
	}
	return {};
}
