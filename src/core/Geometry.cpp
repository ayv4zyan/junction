#include "Geometry.h"

#include <algorithm>

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

qreal dot(QPointF a, QPointF b)
{
	return a.x() * b.x() + a.y() * b.y();
}

// Builds an elbow path one leg at a time. Every choice is whether to keep going the way the
// last leg went or to turn, so it remembers that direction (the heading).
class ElbowPath
{
public:
	explicit ElbowPath(QPointF start) : m_points{start} {}

	const QVector<QPointF> &points() const { return m_points; }

	void lineTo(QPointF p)
	{
		if (p != current())
			m_heading = normalized(p - current());
		m_points.append(p);
	}

	// To reach a Waypoint, keep going the way we were and turn once. If keeping going would
	// run back along the last leg, turn first instead. If the Waypoint is straight behind us
	// (turning first would go nowhere), step aside by a stub, then come back to it.
	void reach(QPointF waypoint)
	{
		const QPointF keepGoing = movingHorizontally() ? horizontallyFirst(waypoint) : verticallyFirst(waypoint);
		const QPointF turnFirst = movingHorizontally() ? verticallyFirst(waypoint) : horizontallyFirst(waypoint);
		if (!goesBack(keepGoing)) {
			lineTo(keepGoing);
		} else if (turnFirst != current()) {
			lineTo(turnFirst);
		} else {
			const QPointF aside = movingHorizontally() ? QPointF(0, ElbowStub) : QPointF(ElbowStub, 0);
			lineTo(current() + aside);
			lineTo(waypoint + aside);
		}
		lineTo(waypoint);
	}

	// Goes to `endStub`, the point a stub out from the end card's `side`. The preferred shape
	// keeps going the way we were. It is no good if its first leg runs back along the last one,
	// or if its last leg travels away from the end card (the path would double back through
	// it). Then the shape that turns first is used; it meets both legs at right angles, so it
	// never has either problem.
	void arriveAt(QPointF endStub, Side side)
	{
		const bool zShape = movingHorizontally() == isHorizontal(side);
		const QVector<QPointF> preferred = cornersTo(endStub, zShape, movingHorizontally());
		const bool headsIntoCard = dot(endStub - preferred.last(), -sideNormal(side)) >= 0;
		const bool usable = !goesBack(preferred.first()) && headsIntoCard;
		for (QPointF corner : usable ? preferred : cornersTo(endStub, zShape, !movingHorizontally()))
			lineTo(corner);
		lineTo(endStub);
	}

private:
	QPointF current() const { return m_points.last(); }
	bool movingHorizontally() const { return m_heading.y() == 0; }
	// Whether a leg to `p` would turn right round and run back along the last leg.
	bool goesBack(QPointF p) const { return dot(p - current(), m_heading) < 0; }
	// The corner on the way to `p` when moving horizontally first, or vertically first.
	QPointF horizontallyFirst(QPointF p) const { return {p.x(), current().y()}; }
	QPointF verticallyFirst(QPointF p) const { return {current().x(), p.y()}; }
	// The corners on the way to `endStub`, setting off horizontally or vertically: with the same
	// direction at both ends a Z, turning halfway; otherwise an L with one corner.
	QVector<QPointF> cornersTo(QPointF endStub, bool zShape, bool horizontalFirst) const
	{
		if (!zShape)
			return {horizontalFirst ? horizontallyFirst(endStub) : verticallyFirst(endStub)};
		const QPointF mid = (current() + endStub) / 2;
		if (horizontalFirst)
			return {horizontallyFirst(mid), QPointF(mid.x(), endStub.y())};
		return {verticallyFirst(mid), QPointF(endStub.x(), mid.y())};
	}

	QVector<QPointF> m_points;
	QPointF m_heading;
};

// Only horizontal and vertical segments. Leaves the start card, passes through each Waypoint
// (turning once to reach it), then turns into the end card.
void elbowRoute(Route &r, const QVector<QPointF> &controls, Side fromSide, Side toSide)
{
	const QPointF start = controls.first();
	const QPointF end = controls.last();

	ElbowPath path(start);
	QVector<int> spanEnds;   // index in the path's points where each span between control points ends
	path.lineTo(start + sideNormal(fromSide) * ElbowStub);
	for (int i = 1; i < controls.size() - 1; ++i) {
		path.reach(controls[i]);
		spanEnds.append(path.points().size() - 1);
	}
	path.arriveAt(end + sideNormal(toSide) * ElbowStub, toSide);
	path.lineTo(end);
	spanEnds.append(path.points().size() - 1);

	int spanStart = 0;
	for (int spanEnd : spanEnds) {
		r.midpoints.append(pointHalfwayAlong(path.points().mid(spanStart, spanEnd - spanStart + 1)));
		spanStart = spanEnd;
	}
	r.path = polylinePath(path.points());
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
	case Side::Floating: break;
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
	case Side::Floating: break;
	}
	return {};
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
	// Floating Ends only look at the two cards, never at Waypoints, so adding or dragging a
	// Waypoint doesn't move an end to another side.
	Side fromSide, toSide;
	if (from.intersects(to)) {
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
	} else {
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
	}
	if (c.fromSide != Side::Floating)
		fromSide = c.fromSide;
	if (c.toSide != Side::Floating)
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
		if (c.fromSide == Side::Floating)
			r.start = borderPoint(from, c.waypoints.isEmpty() ? to.center() : c.waypoints.first());
		if (c.toSide == Side::Floating)
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
	return Side::Floating;
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
