#include "Geometry.h"

#include <QLineF>
#include <QPainterPathStroker>
#include <QtMath>

namespace Geometry {

namespace {

QPointF normalized(QPointF v)
{
    const qreal length = std::hypot(v.x(), v.y());
    return length > 0 ? v / length : QPointF(1, 0);
}

bool isHorizontal(Side side)
{
    return side == Side::Left || side == Side::Right;
}

QPointF pointHalfwayAlong(const QVector<QPointF> &polyline)
{
    qreal total = 0;
    for (int i = 1; i < polyline.size(); ++i)
        total += QLineF(polyline[i - 1], polyline[i]).length();

    qreal remaining = total / 2;
    for (int i = 1; i < polyline.size(); ++i) {
        const QLineF segment(polyline[i - 1], polyline[i]);
        if (segment.length() >= remaining && segment.length() > 0)
            return segment.pointAt(remaining / segment.length());
        remaining -= segment.length();
    }
    return polyline.isEmpty() ? QPointF() : polyline.last();
}

QPainterPath polylinePath(const QVector<QPointF> &points)
{
    QPainterPath path(points.first());
    for (int i = 1; i < points.size(); ++i)
        path.lineTo(points[i]);
    return path;
}

// Straight segments through every control point.
void straightRoute(Route &r, const QVector<QPointF> &controls)
{
    r.path = polylinePath(controls);
    for (int i = 1; i < controls.size(); ++i)
        r.midpoints.append((controls[i - 1] + controls[i]) / 2);
    r.startDirection = normalized(controls[0] - controls[1]);
    r.endDirection = normalized(controls.last() - controls[controls.size() - 2]);
}

// A smooth curve through every control point (Catmull-Rom spline drawn as cubic Béziers).
// At the ends the curve leaves and enters perpendicular to the card side.
void curvedRoute(Route &r, const QVector<QPointF> &controls, QPointF startNormal, QPointF endNormal)
{
    const int n = controls.size();
    auto endStrength = [](QPointF a, QPointF b) {
        return 3 * std::clamp(QLineF(a, b).length() * 0.4, 20.0, 150.0);
    };

    QVector<QPointF> tangents(n);
    tangents[0] = startNormal * endStrength(controls[0], controls[1]);
    tangents[n - 1] = -endNormal * endStrength(controls[n - 2], controls[n - 1]);
    for (int i = 1; i < n - 1; ++i)
        tangents[i] = (controls[i + 1] - controls[i - 1]) / 2;

    r.path = QPainterPath(controls[0]);
    for (int i = 0; i < n - 1; ++i) {
        const QPointF c1 = controls[i] + tangents[i] / 3;
        const QPointF c2 = controls[i + 1] - tangents[i + 1] / 3;
        r.path.cubicTo(c1, c2, controls[i + 1]);
        r.midpoints.append((controls[i] + 3 * c1 + 3 * c2 + controls[i + 1]) / 8);   // Bézier at t = 0.5
    }
    r.startDirection = -startNormal;
    r.endDirection = -endNormal;
}

// Only horizontal and vertical segments. Leaves the start card, passes through each Waypoint
// (turning once to reach it), then turns into the end card.
void elbowRoute(Route &r, const QVector<QPointF> &controls, Side fromSide, Side toSide)
{
    const QPointF start = controls.first();
    const QPointF end = controls.last();
    const QPointF endStub = end + sideNormal(toSide) * ElbowStub;

    QVector<QPointF> points{start};
    QVector<int> spanEnds;   // index in `points` where each span (control point to control point) ends

    QPointF current = start + sideNormal(fromSide) * ElbowStub;
    points.append(current);
    bool movingHorizontally = isHorizontal(fromSide);

    for (int i = 1; i < controls.size() - 1; ++i) {
        const QPointF waypoint = controls[i];
        points.append(movingHorizontally ? QPointF(waypoint.x(), current.y())
                                         : QPointF(current.x(), waypoint.y()));
        points.append(waypoint);
        spanEnds.append(points.size() - 1);
        current = waypoint;
        movingHorizontally = !movingHorizontally;
    }

    // The leg arriving at endStub must not travel away from the end card, or the path would
    // double back through it. When the simple shape would, we turn the other way first.
    const QPointF intoCard = -sideNormal(toSide);
    auto approachesCard = [&](QPointF corner) {
        const QPointF leg = endStub - corner;
        return leg.x() * intoCard.x() + leg.y() * intoCard.y() >= 0;
    };
    const qreal midX = (current.x() + endStub.x()) / 2;
    const qreal midY = (current.y() + endStub.y()) / 2;
    const QPointF turnAtMidX[] = {QPointF(midX, current.y()), QPointF(midX, endStub.y())};
    const QPointF turnAtMidY[] = {QPointF(current.x(), midY), QPointF(endStub.x(), midY)};

    if (movingHorizontally == isHorizontal(toSide)) {
        // Same direction at both ends: a Z shape, turning halfway.
        const QPointF *z = movingHorizontally ? turnAtMidX : turnAtMidY;
        if (!approachesCard(z[1]))
            z = movingHorizontally ? turnAtMidY : turnAtMidX;
        points << z[0] << z[1];
    } else {
        // Perpendicular: an L shape with one corner.
        QPointF corner = movingHorizontally ? QPointF(endStub.x(), current.y()) : QPointF(current.x(), endStub.y());
        if (!approachesCard(corner))
            corner = movingHorizontally ? QPointF(current.x(), endStub.y()) : QPointF(endStub.x(), current.y());
        points << corner;
    }
    points << endStub << end;
    spanEnds.append(points.size() - 1);

    int spanStart = 0;
    for (int spanEnd : spanEnds) {
        r.midpoints.append(pointHalfwayAlong(points.mid(spanStart, spanEnd - spanStart + 1)));
        spanStart = spanEnd;
    }
    r.path = polylinePath(points);
    r.startDirection = -sideNormal(fromSide);
    r.endDirection = -sideNormal(toSide);
}

} // namespace

QPointF sidePoint(const QRectF &rect, Side side)
{
    switch (side) {
    case Side::Top: return {rect.center().x(), rect.top()};
    case Side::Right: return {rect.right(), rect.center().y()};
    case Side::Bottom: return {rect.center().x(), rect.bottom()};
    case Side::Left: return {rect.left(), rect.center().y()};
    case Side::None: break;
    }
    return rect.center();
}

QPointF sideNormal(Side side)
{
    switch (side) {
    case Side::Top: return {0, -1};
    case Side::Right: return {1, 0};
    case Side::Bottom: return {0, 1};
    case Side::Left: return {-1, 0};
    case Side::None: break;
    }
    return {};
}

Side facingSide(const QRectF &rect, QPointF toward)
{
    const QPointF d = toward - rect.center();
    // Compare relative to the card's proportions, so wide cards prefer top/bottom less eagerly.
    const qreal dx = std::abs(d.x()) / std::max(rect.width(), 1.0);
    const qreal dy = std::abs(d.y()) / std::max(rect.height(), 1.0);
    if (dx >= dy)
        return d.x() >= 0 ? Side::Right : Side::Left;
    return d.y() >= 0 ? Side::Bottom : Side::Top;
}

QPointF borderPoint(const QRectF &rect, QPointF toward)
{
    const QPointF d = toward - rect.center();
    if (d.isNull())
        return rect.center();
    // Scale the direction until it touches the nearer of the vertical or horizontal edges.
    const qreal tx = d.x() != 0 ? rect.width() / 2 / std::abs(d.x()) : qInf();
    const qreal ty = d.y() != 0 ? rect.height() / 2 / std::abs(d.y()) : qInf();
    return rect.center() + d * std::min(tx, ty);
}

std::pair<Side, Side> resolveSides(const QRectF &from, const QRectF &to, const Connection &c)
{
    Side fromSide, toSide;
    if (c.waypoints.isEmpty() && from.intersects(to)) {
        // Overlapping, or one inside the other (a card and its Group): there is no gap to
        // cross, so both ends leave through the side of the bigger rect that is nearest to
        // the smaller one. The line goes out and comes back, which keeps it visible.
        const bool fromIsOuter = from.width() * from.height() >= to.width() * to.height();
        const QRectF outer = fromIsOuter ? from : to;
        const QPointF inner = (fromIsOuter ? to : from).center();
        const std::pair<qreal, Side> distances[] = {
            {inner.y() - outer.top(), Side::Top}, {outer.bottom() - inner.y(), Side::Bottom},
            {inner.x() - outer.left(), Side::Left}, {outer.right() - inner.x(), Side::Right}};
        fromSide = toSide = std::min_element(std::begin(distances), std::end(distances))->second;
    } else if (c.waypoints.isEmpty()) {
        // Both ends use the same axis, picked by whichever gap between the cards is wider,
        // so the two ends always face each other.
        const qreal gapX = std::max(to.left() - from.right(), from.left() - to.right());
        const qreal gapY = std::max(to.top() - from.bottom(), from.top() - to.bottom());
        if (gapX >= gapY) {
            const bool toIsRight = to.center().x() >= from.center().x();
            fromSide = toIsRight ? Side::Right : Side::Left;
            toSide = toIsRight ? Side::Left : Side::Right;
        } else {
            const bool toIsBelow = to.center().y() >= from.center().y();
            fromSide = toIsBelow ? Side::Bottom : Side::Top;
            toSide = toIsBelow ? Side::Top : Side::Bottom;
        }
    } else {
        // With Waypoints, each end faces its nearest Waypoint.
        fromSide = facingSide(from, c.waypoints.first());
        toSide = facingSide(to, c.waypoints.last());
    }
    if (c.fromSide != Side::None)
        fromSide = c.fromSide;
    if (c.toSide != Side::None)
        toSide = c.toSide;
    return {fromSide, toSide};
}

Route route(const QRectF &from, const QRectF &to, const Connection &c)
{
    const auto [fromSide, toSide] = resolveSides(from, to, c);

    Route r;
    r.start = sidePoint(from, fromSide);
    r.end = sidePoint(to, toSide);
    if (c.style == PathStyle::Straight && !from.intersects(to)) {
        // A straight line doesn't need to leave perpendicular to a side, so a Floating End
        // sits where the line towards the other end crosses the border. It slides smoothly
        // as cards move instead of jumping between sides.
        if (c.fromSide == Side::None)
            r.start = borderPoint(from, c.waypoints.isEmpty() ? to.center() : c.waypoints.first());
        if (c.toSide == Side::None)
            r.end = borderPoint(to, c.waypoints.isEmpty() ? from.center() : c.waypoints.last());
    }
    const QVector<QPointF> controls = QVector<QPointF>{r.start} + c.waypoints + QVector<QPointF>{r.end};

    switch (c.style) {
    case PathStyle::Straight: straightRoute(r, controls); break;
    case PathStyle::Curved: curvedRoute(r, controls, sideNormal(fromSide), sideNormal(toSide)); break;
    case PathStyle::Elbow: elbowRoute(r, controls, fromSide, toSide); break;
    }
    r.labelPosition = r.path.pointAtPercent(0.5);
    return r;
}

bool hits(const Route &route, QPointF point, qreal tolerance)
{
    QPainterPathStroker stroker;
    stroker.setWidth(tolerance * 2);
    return stroker.createStroke(route.path).contains(point);
}

Side handleAt(const QRectF &rect, QPointF point, qreal radius)
{
    for (Side side : {Side::Top, Side::Right, Side::Bottom, Side::Left}) {
        if (QLineF(sidePoint(rect, side), point).length() <= radius)
            return side;
    }
    return Side::None;
}

qreal snap(qreal value)
{
    return std::round(value / GridSize) * GridSize;
}

QPointF snap(QPointF point)
{
    return {snap(point.x()), snap(point.y())};
}

} // namespace Geometry
