#pragma once

// A Fragment is a piece of a Canvas: some nodes and the Connections between them. Copy puts one
// on the clipboard as JSON Canvas; paste and duplicate insert one.

#include "Document.h"

#include <QJsonObject>
#include <QSet>

struct Fragment
{
	QVector<Node> nodes;               // in document order, so Groups stay behind their Cards
	QVector<Connection> connections;   // each between two of the nodes

	// The nodes named in `nodeIds` and the Connections between two of them.
	static Fragment of(const Document &document, const QSet<QString> &nodeIds);
	// From the clipboard; empty when the JSON isn't a Fragment (such as text from elsewhere).
	static Fragment fromJson(const QJsonObject &json);
	QJsonObject toJson() const;

	bool isEmpty() const { return nodes.isEmpty(); }
	QRectF bounds() const;   // the area the nodes cover

	struct Insertion
	{
		Transaction transaction;   // adds the copies
		QStringList nodeIds;       // the new nodes' ids
	};
	// A copy with fresh ids, moved by `offset`.
	Insertion insertion(QPointF offset) const;
};
