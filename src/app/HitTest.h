#pragma once

// What is at a point on the open Canvas, topmost first (as drawn). Points are in world
// coordinates; QML asks while the pointer moves.

#include "Document.h"

#include <QObject>
#include <QQmlEngine>

class HitTest : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    QML_UNCREATABLE("Created by main.cpp")

public:
    explicit HitTest(const Document *document, QObject *parent = nullptr);

    // The topmost Card under the point. `margin` widens each card so that a point on the outer
    // half of a Handle still counts. Groups are never hit: dropping inside a Group means "new
    // card here".
    Q_INVOKABLE QString cardAt(QPointF point, qreal margin = 0) const;
    // The Handle of the node within `radius` of the point; Floating when there is none.
    Q_INVOKABLE CanvasEnums::Side handleAt(const QString &nodeId, QPointF point, qreal radius) const;
    // The topmost Connection whose line (or label) is within `tolerance` of the point.
    Q_INVOKABLE QString connectionAt(QPointF point, qreal tolerance) const;

private:
    const Document *m_document;
};
