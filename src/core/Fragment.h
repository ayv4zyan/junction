#pragma once

// A Fragment is a piece of a Canvas in JSON Canvas form: some nodes and the Connections between
// them. Copy puts one on the clipboard; paste and duplicate insert one.

#include "Document.h"

#include <QJsonObject>
#include <QSet>

namespace Fragment {

// The nodes named in `nodeIds`, in document order (so Groups stay behind their Cards), and the
// Connections between two of them.
QJsonObject copy(const Document &document, const QSet<QString> &nodeIds);

// The area the Fragment's nodes cover; empty when it has none (text that isn't a Fragment).
QRectF bounds(const QJsonObject &fragment);

struct Insertion
{
    Transaction transaction;   // adds the copies
    QStringList nodeIds;       // the new nodes' ids
};

// A copy of the Fragment with fresh ids, moved by `offset`. Connections to nodes the Fragment
// doesn't contain are left out.
Insertion insertion(const QJsonObject &fragment, QPointF offset);

} // namespace Fragment
