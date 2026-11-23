#include "NodeModel.h"

NodeModel::NodeModel(const Document *document, Filter filter, QObject *parent)
	: QAbstractListModel(parent), m_document(document), m_filter(filter)
{
	connect(document, &Document::nodeAdded, this, [this](const QString &id) {
		if (!accepts(id))
			return;
		beginInsertRows({}, m_ids.size(), m_ids.size());
		m_ids.append(id);
		endInsertRows();
	});
	connect(document, &Document::nodeRemoved, this, [this](const QString &id) {
		const int row = m_ids.indexOf(id);
		if (row < 0)
			return;
		beginRemoveRows({}, row, row);
		m_ids.removeAt(row);
		endRemoveRows();
	});
	// A node whose kind changed (a Card that became a Group) moves to the other model.
	connect(document, &Document::nodeChanged, this, [this](const QString &id) {
		const int row = m_ids.indexOf(id);
		if (row >= 0 && accepts(id))
			emit dataChanged(index(row), index(row));
		else if (row >= 0 || accepts(id))
			resetFromDocument();
	});
	connect(document, &Document::reset, this, &NodeModel::resetFromDocument);
	collectIds();
}

void NodeModel::resetFromDocument()
{
	beginResetModel();
	collectIds();
	endResetModel();
}

bool NodeModel::accepts(const QString &id) const
{
	const Node *n = m_document->node(id);
	return n && n->isCard() == (m_filter == Cards);
}

void NodeModel::collectIds()
{
	m_ids.clear();
	for (const Node &n : m_document->nodes()) {
		if (accepts(n.id))
			m_ids.append(n.id);
	}
}

int NodeModel::rowCount(const QModelIndex &parent) const
{
	return parent.isValid() ? 0 : m_ids.size();
}

QVariant NodeModel::data(const QModelIndex &index, int role) const
{
	const Node *n = index.isValid() ? m_document->node(m_ids.value(index.row())) : nullptr;
	if (!n)
		return {};

	switch (role) {
	case IdRole: return n->id;
	case KindRole: return int(n->kind());   // QML compares it with CanvasEnums.NodeKind
	case XRole: return n->rect.x();
	case YRole: return n->rect.y();
	case WidthRole: return n->rect.width();
	case HeightRole: return n->rect.height();
	case TextRole: return n->text();
	case LabelRole: return n->label();
	case DisplayTextRole: return n->displayText();
	case ColorRole: return n->color;
	}
	return {};
}

QHash<int, QByteArray> NodeModel::roleNames() const
{
	return {
		{IdRole, "nodeId"},     {KindRole, "nodeKind"},
		{XRole, "nodeX"},       {YRole, "nodeY"},
		{WidthRole, "nodeWidth"}, {HeightRole, "nodeHeight"},
		{TextRole, "nodeText"}, {LabelRole, "nodeLabel"},
		{DisplayTextRole, "nodeDisplayText"}, {ColorRole, "nodeColor"},
	};
}
