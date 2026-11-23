#pragma once

#include "Document.h"

#include <QUndoCommand>

// Puts a Transaction on Qt's undo stack.
class TransactionCommand : public QUndoCommand
{
public:
	TransactionCommand(Document *document, Transaction transaction, const QString &text)
		: QUndoCommand(text), m_document(document), m_transaction(std::move(transaction))
	{
	}

	void redo() override { m_document->apply(m_transaction); }
	void undo() override { m_document->apply(inverted(m_transaction)); }

private:
	Document *m_document;
	Transaction m_transaction;
};
