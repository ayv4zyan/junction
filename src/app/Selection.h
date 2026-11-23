#pragma once

// The Selection: the Cards, Groups and Connections the next command acts on, by id, and what
// the menus, the floating toolbar and the Connection's handles need to know about them. It
// belongs to the Editor and follows its Document: ids of items that are gone drop out.

#include "Document.h"

#include <QObject>
#include <QQmlEngine>
#include <QRectF>

// What the right-click menus need to know about the selection.
struct SelectionSummary
{
	Q_GADGET
	QML_VALUE_TYPE(selectionSummary)
	Q_PROPERTY(int cards MEMBER cards)
	Q_PROPERTY(int editableCards MEMBER editableCards)
	Q_PROPERTY(int groups MEMBER groups)
	Q_PROPERTY(int connections MEMBER connections)
	Q_PROPERTY(int labeledConnections MEMBER labeledConnections)
	Q_PROPERTY(QStringList labelTargets MEMBER labelTargets)
	Q_PROPERTY(QString sharedLabel MEMBER sharedLabel)

public:
	int cards = 0;
	int editableCards = 0;        // Text Cards; an Unsupported Card can't be edited
	int groups = 0;
	int connections = 0;
	int labeledConnections = 0;
	QStringList labelTargets;     // the selected items that have a Label: Groups and Connections
	QString sharedLabel;          // the Label they all have, or "" when they differ
};

// Everything the overlay and the toolbar need to edit the one selected Connection.
struct ConnectionDetails
{
	Q_GADGET
	QML_VALUE_TYPE(connectionDetails)
	Q_PROPERTY(QString id MEMBER id)
	Q_PROPERTY(CanvasEnums::PathStyle style MEMBER style)
	Q_PROPERTY(bool fromArrow MEMBER fromArrow)
	Q_PROPERTY(bool toArrow MEMBER toArrow)
	Q_PROPERTY(QString label MEMBER label)
	Q_PROPERTY(QList<QPointF> waypoints MEMBER waypoints)
	Q_PROPERTY(QList<QPointF> midpoints MEMBER midpoints)
	Q_PROPERTY(QPointF start MEMBER start)
	Q_PROPERTY(QPointF end MEMBER end)
	Q_PROPERTY(QPointF labelPosition MEMBER labelPosition)
	Q_PROPERTY(QRectF labelRect MEMBER labelRect)

public:
	QString id;
	PathStyle style = PathStyle::Elbow;
	bool fromArrow = false;
	bool toArrow = false;
	QString label;
	QList<QPointF> waypoints;
	QList<QPointF> midpoints;     // one per span between its ends and Waypoints
	QPointF start;
	QPointF end;
	QPointF labelPosition;
	QRectF labelRect;             // empty without a label
};

class Selection : public QObject
{
	Q_OBJECT
	QML_ELEMENT
	QML_UNCREATABLE("Part of the Editor")

	Q_PROPERTY(QStringList ids READ ids NOTIFY idsChanged)
	Q_PROPERTY(QVariant connection READ connection NOTIFY itemsChanged)
	Q_PROPERTY(QString color READ color NOTIFY itemsChanged)
	Q_PROPERTY(SelectionSummary summary READ summary NOTIFY itemsChanged)
	Q_PROPERTY(QRectF bounds READ bounds NOTIFY itemsChanged)

public:
	explicit Selection(const Document *document, QObject *parent = nullptr);

	QStringList ids() const { return m_ids; }
	bool isEmpty() const { return m_ids.isEmpty(); }
	void set(const QStringList &ids);

	Q_INVOKABLE void select(const QString &id, bool toggle = false);
	// The selection becomes `base` plus what the rectangle catches; called while it is dragged.
	Q_INVOKABLE void selectInRect(const QRectF &rect, const QStringList &base);
	Q_INVOKABLE void selectAll();
	Q_INVOKABLE void clear();

	// The ConnectionDetails of the one selected Connection, or null unless exactly one Connection
	// (with both ends) is selected.
	QVariant connection() const;
	QString color() const;   // of the selected items, "" if mixed or default
	SelectionSummary summary() const;
	// The world rectangle around everything selected (empty when nothing is); the floating
	// selection toolbar sits against it.
	QRectF bounds() const;

signals:
	void idsChanged();
	// The ids, or anything about the selected items (their place, color, label...): it comes
	// with every change to the Document.
	void itemsChanged();

private:
	void followDocument();

	const Document *m_document;
	QStringList m_ids;
};
