#include "Selection.h"

#include "CanvasPainter.h"
#include "DesignValues.h"

Selection::Selection(const Document *document, QObject *parent)
	: QObject(parent), m_document(document)
{
	connect(m_document, &Document::changed, this, &Selection::followDocument);
}

void Selection::set(const QStringList &ids)
{
	if (ids == m_ids)
		return;
	m_ids = ids;
	emit idsChanged();
	emit itemsChanged();
}

// The Document changed: the selection drops the ids of items that are gone (deleted, undone),
// and says the selected items themselves may have changed.
void Selection::followDocument()
{
	QStringList kept;
	for (const QString &id : m_ids) {
		if (m_document->item(id))
			kept.append(id);
	}
	if (kept != m_ids)
		set(kept);
	else
		emit itemsChanged();   // the same items, but something about them changed
}

void Selection::select(const QString &id, bool toggle)
{
	if (!toggle) {
		set({id});
		return;
	}
	QStringList ids = m_ids;
	if (!ids.removeOne(id))
		ids.append(id);
	set(ids);
}

void Selection::selectInRect(const QRectF &rect, const QStringList &base)
{
	// Cards are selected when the rectangle touches them. Groups only when it covers them
	// completely, so a rectangle drawn inside a Group selects its cards, not the Group.
	QStringList ids = base;
	for (const Node &n : m_document->nodes()) {
		const bool hit = n.isCard() ? rect.intersects(n.rect) : rect.contains(n.rect);
		if (hit && !ids.contains(n.id))
			ids.append(n.id);
	}
	set(ids);
}

void Selection::selectAll()
{
	QStringList ids;
	for (const Node &n : m_document->nodes())
		ids.append(n.id);
	for (const Connection &c : m_document->connections())
		ids.append(c.id);
	set(ids);
}

void Selection::clear()
{
	set({});
}

QVariant Selection::connection() const
{
	const Connection *c = m_ids.size() == 1 ? m_document->connection(m_ids.first()) : nullptr;
	const auto route = c ? m_document->route(*c) : std::nullopt;
	if (!route)
		return QVariant::fromValue(nullptr);
	return QVariant::fromValue(ConnectionDetails{
		.id = c->id,
		.style = c->style,
		.fromArrow = c->fromArrow,
		.toArrow = c->toArrow,
		.label = c->label,
		.waypoints = c->waypoints,
		.midpoints = route->midpoints,
		.start = route->start,
		.end = route->end,
		.labelPosition = route->labelPosition,
		.labelRect = CanvasPainter::labelRect(*route, c->label),
	});
}

QString Selection::color() const
{
	QStringList colors;
	for (const QString &id : m_ids) {
		if (const Node *n = m_document->node(id))
			colors.append(n->color);
		else if (const Connection *c = m_document->connection(id))
			colors.append(c->color);
	}
	colors.removeDuplicates();
	return colors.size() == 1 ? colors.first() : QString();
}

SelectionSummary Selection::summary() const
{
	SelectionSummary summary;
	QStringList labels;
	for (const QString &id : m_ids) {
		if (const Node *n = m_document->node(id)) {
			if (n->kind() == NodeKind::Group) {
				++summary.groups;
				summary.labelTargets.append(id);
				labels.append(n->label());
			} else {
				++summary.cards;
				if (n->kind() == NodeKind::Text)
					++summary.editableCards;
			}
		} else if (const Connection *c = m_document->connection(id)) {
			++summary.connections;
			summary.labelTargets.append(id);
			labels.append(c->label);
			if (!c->label.isEmpty())
				++summary.labeledConnections;
		}
	}
	labels.removeDuplicates();
	summary.sharedLabel = labels.size() == 1 ? labels.first() : QString();
	return summary;
}

QRectF Selection::bounds() const
{
	QRectF bounds;
	for (const QString &id : m_ids) {
		if (const Node *n = m_document->node(id)) {
			const qreal labelSpace = n->kind() == NodeKind::Group ? DesignValues::GroupLabelSpace : 0;
			bounds = bounds.united(n->rect.adjusted(0, -labelSpace, 0, 0));
		} else if (const Connection *c = m_document->connection(id)) {
			if (const auto route = m_document->route(*c)) {
				bounds = bounds.united(route->path.boundingRect())
							   .united(CanvasPainter::labelRect(*route, c->label));   // empty without a label
			}
		}
	}
	return bounds;
}
