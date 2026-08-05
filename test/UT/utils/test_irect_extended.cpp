///////////////////////////////////////////////////////////////////
/// Extended test coverage for iRect and iRectF classes
///////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <core/utils/irect.h>
#include <core/utils/ipoint.h>
#include <core/utils/isize.h>

using namespace iShell;

///////////////////////////////////////////////////////////////////
// iRect Tests
///////////////////////////////////////////////////////////////////

// Constructor tests
TEST(iRectExtended, ConstructorWithDimensions) {
    iRect r(10, 20, 100, 50);
    EXPECT_EQ(r.left(), 10);
    EXPECT_EQ(r.top(), 20);
    EXPECT_EQ(r.width(), 100);
    EXPECT_EQ(r.height(), 50);
    EXPECT_EQ(r.right(), 109);  // 10 + 100 - 1
    EXPECT_EQ(r.bottom(), 69);  // 20 + 50 - 1
}

TEST(iRectExtended, ConstructorWithPoints) {
    iPoint topLeft(10, 20);
    iPoint bottomRight(109, 69);
    iRect r(topLeft, bottomRight);
    EXPECT_EQ(r.left(), 10);
    EXPECT_EQ(r.top(), 20);
    EXPECT_EQ(r.right(), 109);
    EXPECT_EQ(r.bottom(), 69);
}

TEST(iRectExtended, ConstructorWithPointAndSize) {
    iPoint topLeft(10, 20);
    iSize size(100, 50);
    iRect r(topLeft, size);
    EXPECT_EQ(r.width(), 100);
    EXPECT_EQ(r.height(), 50);
}

// State tests
TEST(iRectExtended, IsNullCheck) {
    iRect nullRect;
    EXPECT_TRUE(nullRect.isNull());
    EXPECT_TRUE(nullRect.isEmpty());

    iRect validRect(0, 0, 10, 10);
    EXPECT_FALSE(validRect.isNull());
}

TEST(iRectExtended, IsEmptyCheck) {
    iRect emptyRect(10, 20, 0, 0);
    EXPECT_TRUE(emptyRect.isEmpty());

    iRect negativeRect(10, 20, -5, -5);
    EXPECT_TRUE(negativeRect.isEmpty());
}

TEST(iRectExtended, IsValidCheck) {
    iRect validRect(0, 0, 10, 10);
    EXPECT_TRUE(validRect.isValid());

    iRect invalidRect(10, 10, -5, -5);
    EXPECT_FALSE(invalidRect.isValid());
}

// Normalized rectangle
TEST(iRectExtended, NormalizedRect) {
    iRect r(100, 100, -50, -50);
    iRect normalized = r.normalized();
    EXPECT_TRUE(normalized.isValid());
    EXPECT_LE(normalized.left(), normalized.right());
    EXPECT_LE(normalized.top(), normalized.bottom());
}

// Coordinate getters/setters
TEST(iRectExtended, SetCoordinates) {
    iRect r;
    r.setLeft(10);
    r.setTop(20);
    r.setRight(100);
    r.setBottom(80);

    EXPECT_EQ(r.left(), 10);
    EXPECT_EQ(r.top(), 20);
    EXPECT_EQ(r.right(), 100);
    EXPECT_EQ(r.bottom(), 80);
}

TEST(iRectExtended, SetXY) {
    iRect r(0, 0, 50, 50);
    r.setX(100);
    r.setY(200);
    EXPECT_EQ(r.x(), 100);
    EXPECT_EQ(r.y(), 200);
}

// Corner points
TEST(iRectExtended, CornerPoints) {
    iRect r(10, 20, 100, 50);

    EXPECT_EQ(r.topLeft(), iPoint(10, 20));
    EXPECT_EQ(r.topRight(), iPoint(109, 20));
    EXPECT_EQ(r.bottomLeft(), iPoint(10, 69));
    EXPECT_EQ(r.bottomRight(), iPoint(109, 69));
}

TEST(iRectExtended, SetCornerPoints) {
    iRect r;
    r.setTopLeft(iPoint(10, 20));
    r.setBottomRight(iPoint(100, 80));

    EXPECT_EQ(r.left(), 10);
    EXPECT_EQ(r.top(), 20);
    EXPECT_EQ(r.right(), 100);
    EXPECT_EQ(r.bottom(), 80);
}

TEST(iRectExtended, SetTopRightBottomLeft) {
    iRect r(0, 0, 100, 100);
    r.setTopRight(iPoint(200, 50));
    EXPECT_EQ(r.right(), 200);
    EXPECT_EQ(r.top(), 50);

    r.setBottomLeft(iPoint(30, 150));
    EXPECT_EQ(r.left(), 30);
    EXPECT_EQ(r.bottom(), 150);
}

// Center calculation
TEST(iRectExtended, CenterPoint) {
    iRect r(0, 0, 100, 100);
    iPoint center = r.center();
    EXPECT_EQ(center.x(), 49);  // (0 + 99) / 2
    EXPECT_EQ(center.y(), 49);
}

TEST(iRectExtended, CenterWithLargeCoordinates) {
    iRect r(0, 0, 10000, 10000);
    iPoint center = r.center();
    EXPECT_GT(center.x(), 0);
    EXPECT_GT(center.y(), 0);
}

// Move operations
TEST(iRectExtended, MoveEdges) {
    iRect r(10, 20, 100, 50);

    r.moveLeft(50);
    EXPECT_EQ(r.left(), 50);

    r.moveTop(60);
    EXPECT_EQ(r.top(), 60);

    r.moveRight(200);
    EXPECT_EQ(r.right(), 200);

    r.moveBottom(150);
    EXPECT_EQ(r.bottom(), 150);
}

TEST(iRectExtended, MoveCorners) {
    iRect r(10, 20, 100, 50);

    r.moveTopLeft(iPoint(50, 60));
    EXPECT_EQ(r.left(), 50);
    EXPECT_EQ(r.top(), 60);

    r.moveBottomRight(iPoint(200, 150));
    EXPECT_EQ(r.right(), 200);
    EXPECT_EQ(r.bottom(), 150);
}

TEST(iRectExtended, MoveCenter) {
    iRect r(0, 0, 100, 50);
    iPoint newCenter(200, 200);
    r.moveCenter(newCenter);

    iPoint actualCenter = r.center();
    EXPECT_EQ(actualCenter.x(), 200);
    EXPECT_EQ(actualCenter.y(), 200);
}

// Translate operations
TEST(iRectExtended, TranslateByOffset) {
    iRect r(10, 20, 100, 50);
    r.translate(5, 10);

    EXPECT_EQ(r.left(), 15);
    EXPECT_EQ(r.top(), 30);
    EXPECT_EQ(r.width(), 100);
    EXPECT_EQ(r.height(), 50);
}

TEST(iRectExtended, TranslateByPoint) {
    iRect r(10, 20, 100, 50);
    r.translate(iPoint(5, 10));

    EXPECT_EQ(r.left(), 15);
    EXPECT_EQ(r.top(), 30);
}

TEST(iRectExtended, TranslatedCopy) {
    iRect r(10, 20, 100, 50);
    iRect r2 = r.translated(5, 10);

    EXPECT_EQ(r.left(), 10);  // Original unchanged
    EXPECT_EQ(r2.left(), 15);
    EXPECT_EQ(r2.top(), 30);
}

TEST(iRectExtended, TranslatedByPoint) {
    iRect r(10, 20, 100, 50);
    iRect r2 = r.translated(iPoint(5, 10));

    EXPECT_EQ(r2.left(), 15);
    EXPECT_EQ(r2.top(), 30);
}

// Transpose
TEST(iRectExtended, Transposed) {
    iRect r(10, 20, 100, 50);
    iRect t = r.transposed();

    EXPECT_EQ(t.width(), 50);
    EXPECT_EQ(t.height(), 100);
    EXPECT_EQ(t.left(), 10);
    EXPECT_EQ(t.top(), 20);
}

// MoveTo operations
TEST(iRectExtended, MoveToCoordinates) {
    iRect r(10, 20, 100, 50);
    r.moveTo(50, 60);

    EXPECT_EQ(r.left(), 50);
    EXPECT_EQ(r.top(), 60);
    EXPECT_EQ(r.width(), 100);
    EXPECT_EQ(r.height(), 50);
}

TEST(iRectExtended, MoveToPoint) {
    iRect r(10, 20, 100, 50);
    r.moveTo(iPoint(50, 60));

    EXPECT_EQ(r.left(), 50);
    EXPECT_EQ(r.top(), 60);
}

// Rect/Coords getters/setters
TEST(iRectExtended, GetSetRect) {
    iRect r;
    r.setRect(10, 20, 100, 50);

    int x, y, w, h;
    r.getRect(&x, &y, &w, &h);

    EXPECT_EQ(x, 10);
    EXPECT_EQ(y, 20);
    EXPECT_EQ(w, 100);
    EXPECT_EQ(h, 50);
}

TEST(iRectExtended, GetSetCoords) {
    iRect r;
    r.setCoords(10, 20, 109, 69);

    int x1, y1, x2, y2;
    r.getCoords(&x1, &y1, &x2, &y2);

    EXPECT_EQ(x1, 10);
    EXPECT_EQ(y1, 20);
    EXPECT_EQ(x2, 109);
    EXPECT_EQ(y2, 69);
}

// Adjust operations
TEST(iRectExtended, AdjustInPlace) {
    iRect r(10, 20, 100, 50);
    r.adjust(5, 10, -5, -10);

    EXPECT_EQ(r.left(), 15);
    EXPECT_EQ(r.top(), 30);
    EXPECT_EQ(r.right(), 104);  // 109 - 5
    EXPECT_EQ(r.bottom(), 59);  // 69 - 10
}

TEST(iRectExtended, AdjustedCopy) {
    iRect r(10, 20, 100, 50);
    iRect r2 = r.adjusted(5, 10, -5, -10);

    EXPECT_EQ(r.left(), 10);  // Original unchanged
    EXPECT_EQ(r2.left(), 15);
    EXPECT_EQ(r2.top(), 30);
}

// Size operations
TEST(iRectExtended, SizeOperations) {
    iRect r(10, 20, 100, 50);

    EXPECT_EQ(r.size(), iSize(100, 50));
    EXPECT_EQ(r.width(), 100);
    EXPECT_EQ(r.height(), 50);
}

TEST(iRectExtended, SetWidthHeight) {
    iRect r(10, 20, 100, 50);
    r.setWidth(200);
    r.setHeight(80);

    EXPECT_EQ(r.width(), 200);
    EXPECT_EQ(r.height(), 80);
    EXPECT_EQ(r.left(), 10);
    EXPECT_EQ(r.top(), 20);
}

TEST(iRectExtended, SetSize) {
    iRect r(10, 20, 100, 50);
    r.setSize(iSize(200, 80));

    EXPECT_EQ(r.width(), 200);
    EXPECT_EQ(r.height(), 80);
}

// Union operations
TEST(iRectExtended, UnionOperator) {
    iRect r1(10, 20, 100, 50);
    iRect r2(60, 40, 100, 50);
    iRect u = r1 | r2;

    EXPECT_TRUE(u.contains(r1.topLeft()));
    EXPECT_TRUE(u.contains(r1.bottomRight()));
    EXPECT_TRUE(u.contains(r2.topLeft()));
    EXPECT_TRUE(u.contains(r2.bottomRight()));
}

TEST(iRectExtended, UnionAssignment) {
    iRect r1(10, 20, 100, 50);
    iRect r2(60, 40, 100, 50);
    r1 |= r2;

    EXPECT_TRUE(r1.contains(r2.topLeft()));
    EXPECT_TRUE(r1.contains(r2.bottomRight()));
}

TEST(iRectExtended, UnitedMethod) {
    iRect r1(10, 20, 100, 50);
    iRect r2(60, 40, 100, 50);
    iRect u = r1.united(r2);

    EXPECT_TRUE(u.contains(r1.topLeft()));
    EXPECT_TRUE(u.contains(r2.bottomRight()));
}

// Intersection operations
TEST(iRectExtended, IntersectionOperator) {
    iRect r1(10, 20, 100, 50);
    iRect r2(60, 40, 100, 50);
    iRect i = r1 & r2;

    EXPECT_TRUE(i.isValid());
    EXPECT_GE(i.left(), 60);
    EXPECT_LE(i.right(), 109);
}

TEST(iRectExtended, IntersectionAssignment) {
    iRect r1(10, 20, 100, 50);
    iRect r2(60, 40, 100, 50);
    r1 &= r2;

    EXPECT_TRUE(r1.isValid());
    EXPECT_GE(r1.left(), 60);
}

TEST(iRectExtended, IntersectedMethod) {
    iRect r1(10, 20, 100, 50);
    iRect r2(60, 40, 100, 50);
    iRect i = r1.intersected(r2);

    EXPECT_TRUE(i.isValid());
}

TEST(iRectExtended, IntersectsMethod) {
    iRect r1(10, 20, 100, 50);
    iRect r2(60, 40, 100, 50);
    EXPECT_TRUE(r1.intersects(r2));

    iRect r3(200, 200, 50, 50);
    EXPECT_FALSE(r1.intersects(r3));
}

TEST(iRectExtended, NoIntersection) {
    iRect r1(0, 0, 50, 50);
    iRect r2(100, 100, 50, 50);
    iRect i = r1 & r2;

    EXPECT_FALSE(i.isValid());
}

// Contains operations
TEST(iRectExtended, ContainsPoint) {
    iRect r(10, 20, 100, 50);

    EXPECT_TRUE(r.contains(iPoint(50, 40)));
    EXPECT_TRUE(r.contains(iPoint(10, 20)));  // Top-left inclusive
    EXPECT_TRUE(r.contains(iPoint(109, 69))); // Bottom-right inclusive
    EXPECT_FALSE(r.contains(iPoint(5, 15)));
    EXPECT_FALSE(r.contains(iPoint(200, 200)));
}

TEST(iRectExtended, ContainsCoordinates) {
    iRect r(10, 20, 100, 50);

    EXPECT_TRUE(r.contains(50, 40));
    EXPECT_FALSE(r.contains(5, 15));
}

TEST(iRectExtended, ContainsProperPoint) {
    iRect r(10, 20, 100, 50);

    EXPECT_TRUE(r.contains(iPoint(50, 40), false));
    EXPECT_FALSE(r.contains(iPoint(10, 20), true));  // Edge not proper
    EXPECT_FALSE(r.contains(iPoint(109, 69), true)); // Edge not proper
}

TEST(iRectExtended, ContainsRect) {
    iRect r1(10, 20, 100, 50);
    iRect r2(30, 35, 50, 25);

    EXPECT_TRUE(r1.contains(r2));
    EXPECT_FALSE(r2.contains(r1));
}

TEST(iRectExtended, ContainsRectProper) {
    iRect r1(10, 20, 100, 50);
    iRect r2(10, 20, 100, 50);

    EXPECT_TRUE(r1.contains(r2, false));
    EXPECT_FALSE(r1.contains(r2, true));  // Same rect not proper
}

// Comparison operators
TEST(iRectExtended, EqualityOperator) {
    iRect r1(10, 20, 100, 50);
    iRect r2(10, 20, 100, 50);
    iRect r3(10, 20, 101, 50);

    EXPECT_TRUE(r1 == r2);
    EXPECT_FALSE(r1 == r3);
}

TEST(iRectExtended, InequalityOperator) {
    iRect r1(10, 20, 100, 50);
    iRect r2(10, 20, 101, 50);

    EXPECT_TRUE(r1 != r2);
    EXPECT_FALSE(r1 != r1);
}

///////////////////////////////////////////////////////////////////
// iRectF Tests
///////////////////////////////////////////////////////////////////

TEST(iRectFExtended, BasicConstruction) {
    iRectF rf(10.5, 20.5, 100.5, 50.5);
    EXPECT_DOUBLE_EQ(rf.left(), 10.5);
    EXPECT_DOUBLE_EQ(rf.top(), 20.5);
    EXPECT_DOUBLE_EQ(rf.width(), 100.5);
    EXPECT_DOUBLE_EQ(rf.height(), 50.5);
}

TEST(iRectFExtended, ConstructFromIntRect) {
    iRect ri(10, 20, 100, 50);
    iRectF rf(ri);

    EXPECT_DOUBLE_EQ(rf.x(), 10.0);
    EXPECT_DOUBLE_EQ(rf.y(), 20.0);
    EXPECT_DOUBLE_EQ(rf.width(), 100.0);
    EXPECT_DOUBLE_EQ(rf.height(), 50.0);
}

TEST(iRectFExtended, NullAndEmpty) {
    iRectF nullRect;
    EXPECT_TRUE(nullRect.isNull());
    EXPECT_TRUE(nullRect.isEmpty());

    iRectF validRect(0, 0, 10.5, 10.5);
    EXPECT_FALSE(validRect.isNull());
    EXPECT_FALSE(validRect.isEmpty());
    EXPECT_TRUE(validRect.isValid());
}

TEST(iRectFExtended, CenterCalculation) {
    iRectF rf(0, 0, 100, 50);
    iPointF center = rf.center();

    EXPECT_DOUBLE_EQ(center.x(), 50.0);
    EXPECT_DOUBLE_EQ(center.y(), 25.0);
}

TEST(iRectFExtended, TranslateFloat) {
    iRectF rf(10.5, 20.5, 100.5, 50.5);
    rf.translate(5.25, 10.75);

    EXPECT_DOUBLE_EQ(rf.left(), 15.75);
    EXPECT_DOUBLE_EQ(rf.top(), 31.25);
}

TEST(iRectFExtended, ToRect) {
    iRectF rf(10.6, 20.4, 100.5, 50.5);
    iRect ri = rf.toRect();

    // Should round to nearest
    EXPECT_EQ(ri.x(), 11);
    EXPECT_EQ(ri.y(), 20);
}

TEST(iRectFExtended, ToAlignedRect) {
    iRectF rf(10.6, 20.4, 100.5, 50.5);
    iRect ri = rf.toAlignedRect();

    EXPECT_TRUE(ri.isValid());
}

TEST(iRectFExtended, NormalizedFloat) {
    iRectF rf(100.5, 100.5, -50.5, -50.5);
    iRectF normalized = rf.normalized();

    EXPECT_TRUE(normalized.isValid());
}

TEST(iRectFExtended, IntersectsFloat) {
    iRectF r1(10.5, 20.5, 100.5, 50.5);
    iRectF r2(60.5, 40.5, 100.5, 50.5);

    EXPECT_TRUE(r1.intersects(r2));
}

TEST(iRectFExtended, ContainsPointFloat) {
    iRectF rf(10.5, 20.5, 100.5, 50.5);

    EXPECT_TRUE(rf.contains(iPointF(50.5, 40.5)));
    EXPECT_FALSE(rf.contains(iPointF(5.5, 15.5)));
}

TEST(iRectFExtended, EqualityFloat) {
    iRectF r1(10.5, 20.5, 100.5, 50.5);
    iRectF r2(10.5, 20.5, 100.5, 50.5);
    iRectF r3(10.6, 20.5, 100.5, 50.5);

    EXPECT_TRUE(r1 == r2);
    EXPECT_FALSE(r1 == r3);
    EXPECT_TRUE(r1 != r3);
}

// Edge cases
TEST(iRectExtended, ZeroSizeRect) {
    iRect r(10, 20, 0, 0);
    EXPECT_TRUE(r.isEmpty());
    EXPECT_FALSE(r.isValid());
}

TEST(iRectExtended, SinglePixelRect) {
    iRect r(10, 20, 1, 1);
    EXPECT_TRUE(r.isValid());
    EXPECT_EQ(r.width(), 1);
    EXPECT_EQ(r.height(), 1);
}

TEST(iRectExtended, NegativeSizeHandling) {
    iRect r(10, 20, -50, -30);
    EXPECT_FALSE(r.isValid());
    EXPECT_TRUE(r.isEmpty());

    iRect normalized = r.normalized();
    EXPECT_TRUE(normalized.isValid());
}

// Rectangle-vs-rectangle union / intersection / containment (float)
TEST(iRectFExtended, UnitedFloat) {
    iRectF r1(0.0, 0.0, 10.0, 10.0);
    iRectF r2(5.0, 5.0, 10.0, 10.0);

    iRectF u = r1 | r2;                 // exercises operator|
    EXPECT_DOUBLE_EQ(u.x(), 0.0);
    EXPECT_DOUBLE_EQ(u.y(), 0.0);
    EXPECT_DOUBLE_EQ(u.width(), 15.0);
    EXPECT_DOUBLE_EQ(u.height(), 15.0);

    // united() delegates to operator|
    EXPECT_EQ(u, r1.united(r2));

    // union with a null rect returns the other operand
    iRectF nullR;
    EXPECT_EQ(r1, r1 | nullR);
    EXPECT_EQ(r1, nullR | r1);
}

TEST(iRectFExtended, IntersectedFloat) {
    iRectF r1(0.0, 0.0, 10.0, 10.0);
    iRectF r2(5.0, 5.0, 10.0, 10.0);

    iRectF i = r1 & r2;                 // exercises operator&
    EXPECT_DOUBLE_EQ(i.x(), 5.0);
    EXPECT_DOUBLE_EQ(i.y(), 5.0);
    EXPECT_DOUBLE_EQ(i.width(), 5.0);
    EXPECT_DOUBLE_EQ(i.height(), 5.0);

    // intersected() delegates to operator&
    EXPECT_EQ(i, r1.intersected(r2));

    // non-overlapping rectangles produce a null intersection
    iRectF far(100.0, 100.0, 5.0, 5.0);
    EXPECT_TRUE((r1 & far).isNull());
}

TEST(iRectFExtended, ContainsRectFloat) {
    iRectF outer(0.0, 0.0, 100.0, 100.0);
    iRectF inner(10.0, 10.0, 20.0, 20.0);

    EXPECT_TRUE(outer.contains(inner));
    EXPECT_FALSE(inner.contains(outer));

    // partial overlap is not containment
    iRectF overlap(50.0, 50.0, 100.0, 100.0);
    EXPECT_FALSE(outer.contains(overlap));

    // a null argument is never contained
    iRectF nullR;
    EXPECT_FALSE(outer.contains(nullR));
}

// Branch coverage: normalization, reversed rects, proper containment (int)
TEST(iRectExtended, NormalizedAlreadyNormal) {
    iRect r(10, 20, 30, 40);
    EXPECT_EQ(r, r.normalized());
}

TEST(iRectExtended, ContainsPointProperAndReversed) {
    iRect r(0, 0, 100, 100);
    EXPECT_TRUE(r.contains(iPoint(50, 50), true));
    EXPECT_FALSE(r.contains(iPoint(0, 0), true));    // on x edge, not proper
    EXPECT_FALSE(r.contains(iPoint(50, 0), true));   // on y edge, not proper

    // reversed rect (internally negative coords)
    iRect rev(100, 100, -100, -100);
    EXPECT_TRUE(rev.contains(iPoint(50, 50)));
    EXPECT_FALSE(rev.contains(iPoint(200, 50)));
    EXPECT_FALSE(rev.contains(iPoint(50, 200)));
}

TEST(iRectExtended, ContainsRectProperAndReversed) {
    iRect outer(0, 0, 100, 100);
    iRect inner(10, 10, 20, 20);
    EXPECT_TRUE(outer.contains(inner, true));
    EXPECT_FALSE(outer.contains(iRect(10, 0, 20, 100), true));  // y not proper
    EXPECT_FALSE(outer.contains(iRect()));                      // null argument

    iRect rev(100, 100, -100, -100);
    EXPECT_TRUE(rev.contains(iRect(10, 10, 20, 20)));
    EXPECT_TRUE(rev.contains(iRect(30, 30, -20, -20)));         // reversed argument
}

TEST(iRectExtended, UnionAndIntersectReversed) {
    iRect nullR;
    iRect r(0, 0, 10, 10);
    EXPECT_EQ(r, nullR | r);   // null left
    EXPECT_EQ(r, r | nullR);   // null right

    iRect rev(20, 20, -20, -20);
    EXPECT_FALSE((rev | iRect(0, 0, 5, 5)).isNull());

    iRect a(0, 0, 10, 10);
    EXPECT_TRUE((a & iRect(100, 100, 10, 10)).isNull());  // x disjoint
    EXPECT_TRUE((a & iRect(0, 100, 10, 10)).isNull());    // y disjoint
    EXPECT_FALSE((rev & iRect(0, 0, 30, 30)).isNull());   // reversed intersect
    EXPECT_FALSE((rev & iRect(5, 5, -3, -3)).isNull());   // both reversed
}

// Branch coverage: iRectF normalization, negative dims, null rects
TEST(iRectFExtended, NormalizedNegative) {
    iRectF r(10.0, 20.0, -5.0, -8.0);
    iRectF n = r.normalized();
    EXPECT_DOUBLE_EQ(n.x(), 5.0);
    EXPECT_DOUBLE_EQ(n.y(), 12.0);
    EXPECT_DOUBLE_EQ(n.width(), 5.0);
    EXPECT_DOUBLE_EQ(n.height(), 8.0);
}

TEST(iRectFExtended, ContainsPointNegativeAndNull) {
    iRectF rev(10.0, 10.0, -10.0, -10.0);
    EXPECT_TRUE(rev.contains(iPointF(5.0, 5.0)));
    EXPECT_FALSE(rev.contains(iPointF(50.0, 5.0)));
    EXPECT_FALSE(rev.contains(iPointF(5.0, 50.0)));

    iRectF nullW(0.0, 0.0, 0.0, 10.0);
    EXPECT_FALSE(nullW.contains(iPointF(0.0, 5.0)));
    iRectF nullH(0.0, 0.0, 10.0, 0.0);
    EXPECT_FALSE(nullH.contains(iPointF(5.0, 0.0)));
}

TEST(iRectFExtended, ContainsRectNegativeAndNull) {
    iRectF rev(20.0, 20.0, -20.0, -20.0);
    EXPECT_TRUE(rev.contains(iRectF(5.0, 5.0, 5.0, 5.0)));
    EXPECT_TRUE(rev.contains(iRectF(10.0, 10.0, -5.0, -5.0)));

    iRectF nullSelf(0.0, 0.0, 0.0, 10.0);
    EXPECT_FALSE(nullSelf.contains(iRectF(0.0, 0.0, 1.0, 1.0)));
    iRectF outer(0.0, 0.0, 100.0, 100.0);
    EXPECT_FALSE(outer.contains(iRectF(0.0, 0.0, 0.0, 5.0)));
    EXPECT_FALSE(outer.contains(iRectF(0.0, 0.0, 5.0, 0.0)));
    EXPECT_FALSE(outer.contains(iRectF(-5.0, 0.0, 10.0, 10.0)));
    EXPECT_FALSE(outer.contains(iRectF(0.0, -5.0, 10.0, 10.0)));
}

TEST(iRectFExtended, UnionIntersectNegative) {
    iRectF rev(20.0, 20.0, -20.0, -20.0);
    EXPECT_FALSE((rev | iRectF(10.0, 10.0, -5.0, -5.0)).isNull());

    iRectF nullW(0.0, 0.0, 0.0, 10.0);
    iRectF a(0.0, 0.0, 10.0, 10.0);
    EXPECT_TRUE((nullW & iRectF(0.0, 0.0, 5.0, 5.0)).isNull());
    EXPECT_TRUE((a & iRectF(5.0, 5.0, 0.0, 5.0)).isNull());
    EXPECT_FALSE((rev & iRectF(5.0, 5.0, -3.0, -3.0)).isNull());
    EXPECT_TRUE((a & iRectF(100.0, 0.0, 10.0, 10.0)).isNull());
    EXPECT_TRUE((a & iRectF(0.0, 100.0, 10.0, 10.0)).isNull());

    EXPECT_TRUE(rev.intersects(iRectF(5.0, 5.0, -3.0, -3.0)));
    EXPECT_FALSE(nullW.intersects(a));
    EXPECT_FALSE(a.intersects(iRectF(5.0, 5.0, 0.0, 5.0)));
    EXPECT_FALSE(a.intersects(iRectF(100.0, 0.0, 5.0, 5.0)));
    EXPECT_FALSE(a.intersects(iRectF(0.0, 100.0, 5.0, 5.0)));
}
