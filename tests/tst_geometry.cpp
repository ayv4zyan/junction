#include "Geometry.h"

#include <QTest>

class TestGeometry : public QObject
{
	Q_OBJECT

	static Connection connection(PathStyle style, QVector<QPointF> waypoints = {})
	{
		Connection c;
		c.style = style;
		c.waypoints = waypoints;
		return c;
	}

	// Every segment of an elbow path must be horizontal or vertical.
	static bool isOrthogonal(const QPainterPath &path)
	{
		for (int i = 1; i < path.elementCount(); ++i) {
			const QPointF a = path.elementAt(i - 1), b = path.elementAt(i);
			if (!qFuzzyCompare(a.x(), b.x()) && !qFuzzyCompare(a.y(), b.y()))
				return false;
		}
		return true;
	}

	// Whether a path turns right round somewhere and runs back along itself.
	static bool doublesBack(const QPainterPath &path)
	{
		for (int i = 2; i < path.elementCount(); ++i) {
			const QPointF a = path.elementAt(i - 2), b = path.elementAt(i - 1), c = path.elementAt(i);
			const QPointF ab = b - a, bc = c - b;
			if (ab.x() * bc.x() + ab.y() * bc.y() < 0)
				return true;
		}
		return false;
	}

private slots:
	void floatingEndsFaceEachOther()
	{
		// The situation from the Obsidian screenshot: one card up-right of the other.
		const QRectF top(300, 0, 250, 60), bottom(0, 140, 250, 60);
		const auto [fromSide, toSide] = Geometry::resolveSides(top, bottom, Connection{});
		QCOMPARE(fromSide, Side::Bottom);
		QCOMPARE(toSide, Side::Top);
	}

	// Both floating ends pick the same axis, so they always face each other.
	void floatingEndsAgree_data()
	{
		QTest::addColumn<QRectF>("to");
		QTest::addColumn<int>("fromSide");
		QTest::addColumn<int>("toSide");
		// your screenshot: a taller card up-left; the per-end rule gave "top" on one, "right" on the other
		QTest::newRow("diagonal") << QRectF(-270, -110, 240, 100) << int(Side::Left) << int(Side::Right);
		QTest::newRow("below") << QRectF(-100, 200, 240, 100) << int(Side::Bottom) << int(Side::Top);
	}

	void floatingEndsAgree()
	{
		QFETCH(QRectF, to);
		QFETCH(int, fromSide);
		QFETCH(int, toSide);
		const auto [f, t] = Geometry::resolveSides({0, 0, 240, 80}, to, Connection{});
		QCOMPARE(int(f), fromSide);
		QCOMPARE(int(t), toSide);
	}

	// A straight floating end slides along the border: a small move never makes it jump.
	void straightEndsMoveSmoothly()
	{
		const QRectF api(0, 0, 240, 100);
		QPointF previous;
		for (int i = 0; i <= 100; ++i) {
			const QRectF client(500 - 3 * i, 150, 240, 80);   // slides left, past the point where sides used to flip
			const auto route = Geometry::route(client, api, connection(PathStyle::Straight));
			if (i > 0)
				QVERIFY2(QLineF(previous, route.start).length() < 5,
						 qPrintable(QString("jumped at step %1").arg(i)));
			previous = route.start;
		}
	}

	void pinnedSideIsKept()
	{
		Connection c;
		c.fromSide = Side::Top;
		const auto [fromSide, toSide] = Geometry::resolveSides({0, 0, 100, 50}, {300, 0, 100, 50}, c);
		QCOMPARE(fromSide, Side::Top);
		QCOMPARE(toSide, Side::Left);
	}

	void elbowIsOrthogonal_data()
	{
		QTest::addColumn<QRectF>("to");
		QTest::addColumn<QVector<QPointF>>("waypoints");
		QTest::newRow("side by side") << QRectF(300, 40, 100, 50) << QVector<QPointF>{};
		QTest::newRow("diagonal") << QRectF(300, 300, 100, 50) << QVector<QPointF>{};
		QTest::newRow("waypoints") << QRectF(300, 300, 100, 50) << QVector<QPointF>{{150, -100}, {500, 100}};
		QTest::newRow("waypoint below the gap") << QRectF(300, 0, 100, 50) << QVector<QPointF>{{200, 300}};
		QTest::newRow("waypoint above, then below") << QRectF(400, 0, 100, 50) << QVector<QPointF>{{200, -200}, {300, 300}};
	}

	void elbowIsOrthogonal()
	{
		QFETCH(QRectF, to);
		QFETCH(QVector<QPointF>, waypoints);
		const auto route = Geometry::route({0, 0, 100, 50}, to, connection(PathStyle::Elbow, waypoints));
		QVERIFY(isOrthogonal(route.path));
		QVERIFY(!doublesBack(route.path));
		QCOMPARE(route.midpoints.size(), waypoints.size() + 1);
	}

	// A Waypoint straight behind a pinned start, on the very line the path leaves on: the path
	// steps aside to reach it instead of running back over itself.
	void elbowStepsAsideForWaypointBehind()
	{
		Connection c = connection(PathStyle::Elbow, {{-80, 25}});
		c.fromSide = Side::Right;
		const auto route = Geometry::route({0, 0, 100, 50}, {300, 300, 100, 50}, c);
		QVERIFY(isOrthogonal(route.path));
		QVERIFY(!doublesBack(route.path));
	}

	// Pinned sides that face away from each other must route around the cards, not through them.
	void elbowStaysOutOfCards_data()
	{
		QTest::addColumn<QRectF>("from");
		QTest::addColumn<int>("fromSide");
		QTest::addColumn<QRectF>("to");
		QTest::addColumn<int>("toSide");
		// the Obsidian screenshot: right side of the upper card to the left side of the lower-left card
		QTest::newRow("right to left, target behind") << QRectF(300, 0, 250, 60) << int(Side::Right)
													 << QRectF(0, 140, 250, 60) << int(Side::Left);
		QTest::newRow("bottom to right, target below-left") << QRectF(400, 0, 240, 80) << int(Side::Bottom)
														   << QRectF(300, 120, 240, 80) << int(Side::Right);
		QTest::newRow("top to top") << QRectF(0, 200, 240, 80) << int(Side::Top)
									<< QRectF(400, 0, 240, 80) << int(Side::Top);
	}

	void elbowStaysOutOfCards()
	{
		QFETCH(QRectF, from);
		QFETCH(int, fromSide);
		QFETCH(QRectF, to);
		QFETCH(int, toSide);
		Connection c = connection(PathStyle::Elbow);
		c.fromSide = Side(fromSide);
		c.toSide = Side(toSide);
		const auto route = Geometry::route(from, to, c);
		QVERIFY(isOrthogonal(route.path));
		for (int i = 1; i < 100; ++i) {
			const QPointF p = route.path.pointAtPercent(i / 100.0);
			QVERIFY2(!from.adjusted(1, 1, -1, -1).contains(p) && !to.adjusted(1, 1, -1, -1).contains(p),
					 qPrintable(QString("path goes through a card at %1,%2").arg(p.x()).arg(p.y())));
		}
	}

	// A card connected to a Group around it (possible in Obsidian files), or to an overlapping card.
	void overlappingRectsGiveVisibleLines_data()
	{
		QTest::addColumn<QRectF>("to");
		QTest::newRow("group around card") << QRectF(-100, -100, 600, 400);
		QTest::newRow("overlapping card") << QRectF(100, 40, 240, 80);
	}

	void overlappingRectsGiveVisibleLines()
	{
		QFETCH(QRectF, to);
		const QRectF card(40, 40, 240, 80);
		for (PathStyle style : {PathStyle::Straight, PathStyle::Curved, PathStyle::Elbow}) {
			const auto route = Geometry::route(card, to, connection(style));
			QVERIFY2(route.path.length() > 20, qPrintable(pathStyleToString(style)));
			QVERIFY(!route.endDirection.isNull());
			// both ends use the same side, so the line leaves and arrives in the same direction
			const auto [fromSide, toSide] = Geometry::resolveSides(card, to, connection(style));
			QCOMPARE(fromSide, toSide);
		}
	}

	// Adding a Waypoint, wherever it is dragged, must not move a Floating End to another side.
	void waypointsDontChangeFloatingSides()
	{
		const QRectF left(0, 0, 100, 50), right(400, 0, 100, 50);
		for (QPointF waypoint : {QPointF(250, 300), QPointF(250, -300), QPointF(-200, 25)}) {
			const auto [fromSide, toSide] = Geometry::resolveSides(left, right, connection(PathStyle::Elbow, {waypoint}));
			QCOMPARE(fromSide, Side::Right);
			QCOMPARE(toSide, Side::Left);
		}
	}

	void pathsPassThroughWaypoints()
	{
		const QPointF waypoint(200, 300);
		for (PathStyle style : {PathStyle::Straight, PathStyle::Curved, PathStyle::Elbow}) {
			const auto route = Geometry::route({0, 0, 100, 50}, {400, 0, 100, 50}, connection(style, {waypoint}));
			QVERIFY2(Geometry::hits(route, waypoint, 0.5), qPrintable(pathStyleToString(style)));
		}
	}

	void hitTesting()
	{
		const auto route = Geometry::route({0, 0, 100, 50}, {300, 0, 100, 50}, connection(PathStyle::Straight));
		QVERIFY(Geometry::hits(route, {200, 27}, 4));
		QVERIFY(!Geometry::hits(route, {200, 60}, 4));
	}

	// The selection toolbar: centred above the selection, 8px away, inside an 800x600 view.

	void handles()
	{
		const QRectF card(0, 0, 100, 50);
		QCOMPARE(Geometry::handleAt(card, {101, 24}, 8), Side::Right);
		QCOMPARE(Geometry::handleAt(card, {50, 25}, 8), Side::Floating);
	}
};

QTEST_GUILESS_MAIN(TestGeometry)
#include "tst_geometry.moc"
