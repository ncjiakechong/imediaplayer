/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    irect.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IRECT_H
#define IRECT_H

#include <core/utils/ipoint.h>
#include <core/utils/isize.h>

namespace iShell {

class iRect
{
public:
    iRect() : x1(0), y1(0), x2(-1), y2(-1) {}
    iRect(const iPoint &topleft, const iPoint &bottomright);
    iRect(const iPoint &topleft, const iSize &size);
    iRect(int left, int top, int width, int height);

    inline bool isNull() const;
    inline bool isEmpty() const;
    inline bool isValid() const;

    inline int left() const;
    inline int top() const;
    inline int right() const;
    inline int bottom() const;
    iRect normalized() const;

    inline int x() const;
    inline int y() const;
    inline void setLeft(int pos);
    inline void setTop(int pos);
    inline void setRight(int pos);
    inline void setBottom(int pos);
    inline void setX(int x);
    inline void setY(int y);

    inline void setTopLeft(const iPoint &p);
    inline void setBottomRight(const iPoint &p);
    inline void setTopRight(const iPoint &p);
    inline void setBottomLeft(const iPoint &p);

    inline iPoint topLeft() const;
    inline iPoint bottomRight() const;
    inline iPoint topRight() const;
    inline iPoint bottomLeft() const;
    inline iPoint center() const;

    inline void moveLeft(int pos);
    inline void moveTop(int pos);
    inline void moveRight(int pos);
    inline void moveBottom(int pos);
    inline void moveTopLeft(const iPoint &p);
    inline void moveBottomRight(const iPoint &p);
    inline void moveTopRight(const iPoint &p);
    inline void moveBottomLeft(const iPoint &p);
    inline void moveCenter(const iPoint &p);

    inline void translate(int dx, int dy);
    inline void translate(const iPoint &p);
    inline iRect translated(int dx, int dy) const;
    inline iRect translated(const iPoint &p) const;
    inline iRect transposed() const;

    inline void moveTo(int x, int t);
    inline void moveTo(const iPoint &p);

    inline void setRect(int x, int y, int w, int h);
    inline void getRect(int *x, int *y, int *w, int *h) const;

    inline void setCoords(int x1, int y1, int x2, int y2);
    inline void getCoords(int *x1, int *y1, int *x2, int *y2) const;

    inline void adjust(int x1, int y1, int x2, int y2);
    inline iRect adjusted(int x1, int y1, int x2, int y2) const;

    inline iSize size() const;
    inline int width() const;
    inline int height() const;
    inline void setWidth(int w);
    inline void setHeight(int h);
    inline void setSize(const iSize &s);

    iRect operator|(const iRect &r) const;
    iRect operator&(const iRect &r) const;
    inline iRect& operator|=(const iRect &r);
    inline iRect& operator&=(const iRect &r);

    bool contains(const iRect &r, bool proper = false) const;
    bool contains(const iPoint &p, bool proper=false) const;
    inline bool contains(int x, int y) const;
    inline bool contains(int x, int y, bool proper) const;
    inline iRect united(const iRect &other) const;
    inline iRect intersected(const iRect &other) const;
    bool intersects(const iRect &r) const;

    iRect unite(const iRect &r) const { return united(r); }
    iRect intersect(const iRect &r) const { return intersected(r); }

    friend inline bool operator==(const iRect &, const iRect &);
    friend inline bool operator!=(const iRect &, const iRect &);

private:
    int x1;
    int y1;
    int x2;
    int y2;
};

inline bool operator==(const iRect &, const iRect &);
inline bool operator!=(const iRect &, const iRect &);

/*****************************************************************************
  iRect inline member functions
 *****************************************************************************/

inline iRect::iRect(int aleft, int atop, int awidth, int aheight)
    : x1(aleft), y1(atop), x2(aleft + awidth - 1), y2(atop + aheight - 1) {}

inline iRect::iRect(const iPoint &atopLeft, const iPoint &abottomRight)
    : x1(atopLeft.x()), y1(atopLeft.y()), x2(abottomRight.x()), y2(abottomRight.y()) {}

inline iRect::iRect(const iPoint &atopLeft, const iSize &asize)
    : x1(atopLeft.x()), y1(atopLeft.y()), x2(atopLeft.x()+asize.width() - 1), y2(atopLeft.y()+asize.height() - 1) {}

inline bool iRect::isNull() const
{ return x2 == x1 - 1 && y2 == y1 - 1; }

inline bool iRect::isEmpty() const
{ return x1 > x2 || y1 > y2; }

inline bool iRect::isValid() const
{ return x1 <= x2 && y1 <= y2; }

inline int iRect::left() const
{ return x1; }

inline int iRect::top() const
{ return y1; }

inline int iRect::right() const
{ return x2; }

inline int iRect::bottom() const
{ return y2; }

inline int iRect::x() const
{ return x1; }

inline int iRect::y() const
{ return y1; }

inline void iRect::setLeft(int pos)
{ x1 = pos; }

inline void iRect::setTop(int pos)
{ y1 = pos; }

inline void iRect::setRight(int pos)
{ x2 = pos; }

inline void iRect::setBottom(int pos)
{ y2 = pos; }

inline void iRect::setTopLeft(const iPoint &p)
{ x1 = p.x(); y1 = p.y(); }

inline void iRect::setBottomRight(const iPoint &p)
{ x2 = p.x(); y2 = p.y(); }

inline void iRect::setTopRight(const iPoint &p)
{ x2 = p.x(); y1 = p.y(); }

inline void iRect::setBottomLeft(const iPoint &p)
{ x1 = p.x(); y2 = p.y(); }

inline void iRect::setX(int ax)
{ x1 = ax; }

inline void iRect::setY(int ay)
{ y1 = ay; }

inline iPoint iRect::topLeft() const
{ return iPoint(x1, y1); }

inline iPoint iRect::bottomRight() const
{ return iPoint(x2, y2); }

inline iPoint iRect::topRight() const
{ return iPoint(x2, y1); }

inline iPoint iRect::bottomLeft() const
{ return iPoint(x1, y2); }

inline iPoint iRect::center() const
{ return iPoint(int((xint64(x1)+x2)/2), int((xint64(y1)+y2)/2)); } // cast avoids overflow on addition

inline int iRect::width() const
{ return  x2 - x1 + 1; }

inline int iRect::height() const
{ return  y2 - y1 + 1; }

inline iSize iRect::size() const
{ return iSize(width(), height()); }

inline void iRect::translate(int dx, int dy)
{
    x1 += dx;
    y1 += dy;
    x2 += dx;
    y2 += dy;
}

inline void iRect::translate(const iPoint &p)
{
    x1 += p.x();
    y1 += p.y();
    x2 += p.x();
    y2 += p.y();
}

inline iRect iRect::translated(int dx, int dy) const
{ return iRect(iPoint(x1 + dx, y1 + dy), iPoint(x2 + dx, y2 + dy)); }

inline iRect iRect::translated(const iPoint &p) const
{ return iRect(iPoint(x1 + p.x(), y1 + p.y()), iPoint(x2 + p.x(), y2 + p.y())); }

inline iRect iRect::transposed() const
{ return iRect(topLeft(), size().transposed()); }

inline void iRect::moveTo(int ax, int ay)
{
    x2 += ax - x1;
    y2 += ay - y1;
    x1 = ax;
    y1 = ay;
}

inline void iRect::moveTo(const iPoint &p)
{
    x2 += p.x() - x1;
    y2 += p.y() - y1;
    x1 = p.x();
    y1 = p.y();
}

inline void iRect::moveLeft(int pos)
{ x2 += (pos - x1); x1 = pos; }

inline void iRect::moveTop(int pos)
{ y2 += (pos - y1); y1 = pos; }

inline void iRect::moveRight(int pos)
{
    x1 += (pos - x2);
    x2 = pos;
}

inline void iRect::moveBottom(int pos)
{
    y1 += (pos - y2);
    y2 = pos;
}

inline void iRect::moveTopLeft(const iPoint &p)
{
    moveLeft(p.x());
    moveTop(p.y());
}

inline void iRect::moveBottomRight(const iPoint &p)
{
    moveRight(p.x());
    moveBottom(p.y());
}

inline void iRect::moveTopRight(const iPoint &p)
{
    moveRight(p.x());
    moveTop(p.y());
}

inline void iRect::moveBottomLeft(const iPoint &p)
{
    moveLeft(p.x());
    moveBottom(p.y());
}

inline void iRect::moveCenter(const iPoint &p)
{
    int w = x2 - x1;
    int h = y2 - y1;
    x1 = p.x() - w/2;
    y1 = p.y() - h/2;
    x2 = x1 + w;
    y2 = y1 + h;
}

inline void iRect::getRect(int *ax, int *ay, int *aw, int *ah) const
{
    *ax = x1;
    *ay = y1;
    *aw = x2 - x1 + 1;
    *ah = y2 - y1 + 1;
}

inline void iRect::setRect(int ax, int ay, int aw, int ah)
{
    x1 = ax;
    y1 = ay;
    x2 = (ax + aw - 1);
    y2 = (ay + ah - 1);
}

inline void iRect::getCoords(int *xp1, int *yp1, int *xp2, int *yp2) const
{
    *xp1 = x1;
    *yp1 = y1;
    *xp2 = x2;
    *yp2 = y2;
}

inline void iRect::setCoords(int xp1, int yp1, int xp2, int yp2)
{
    x1 = xp1;
    y1 = yp1;
    x2 = xp2;
    y2 = yp2;
}

inline iRect iRect::adjusted(int xp1, int yp1, int xp2, int yp2) const
{ return iRect(iPoint(x1 + xp1, y1 + yp1), iPoint(x2 + xp2, y2 + yp2)); }

inline void iRect::adjust(int dx1, int dy1, int dx2, int dy2)
{
    x1 += dx1;
    y1 += dy1;
    x2 += dx2;
    y2 += dy2;
}

inline void iRect::setWidth(int w)
{ x2 = (x1 + w - 1); }

inline void iRect::setHeight(int h)
{ y2 = (y1 + h - 1); }

inline void iRect::setSize(const iSize &s)
{
    x2 = (s.width()  + x1 - 1);
    y2 = (s.height() + y1 - 1);
}

inline bool iRect::contains(int ax, int ay, bool aproper) const
{
    return contains(iPoint(ax, ay), aproper);
}

inline bool iRect::contains(int ax, int ay) const
{
    return contains(iPoint(ax, ay), false);
}

inline iRect& iRect::operator|=(const iRect &r)
{
    *this = *this | r;
    return *this;
}

inline iRect& iRect::operator&=(const iRect &r)
{
    *this = *this & r;
    return *this;
}

inline iRect iRect::intersected(const iRect &other) const
{
    return *this & other;
}

inline iRect iRect::united(const iRect &r) const
{
    return *this | r;
}

inline bool operator==(const iRect &r1, const iRect &r2)
{
    return r1.x1==r2.x1 && r1.x2==r2.x2 && r1.y1==r2.y1 && r1.y2==r2.y2;
}

inline bool operator!=(const iRect &r1, const iRect &r2)
{
    return r1.x1!=r2.x1 || r1.x2!=r2.x2 || r1.y1!=r2.y1 || r1.y2!=r2.y2;
}

class iRectF
{
public:
    iRectF() : xp(0.), yp(0.), w(0.), h(0.) {}
    iRectF(const iPointF &topleft, const iSizeF &size);
    iRectF(const iPointF &topleft, const iPointF &bottomRight);
    iRectF(double left, double top, double width, double height);
    iRectF(const iRect &rect);

    inline bool isNull() const;
    inline bool isEmpty() const;
    inline bool isValid() const;
    iRectF normalized() const;

    inline double left() const { return xp; }
    inline double top() const { return yp; }
    inline double right() const { return xp + w; }
    inline double bottom() const { return yp + h; }

    inline double x() const;
    inline double y() const;
    inline void setLeft(double pos);
    inline void setTop(double pos);
    inline void setRight(double pos);
    inline void setBottom(double pos);
    inline void setX(double pos) { setLeft(pos); }
    inline void setY(double pos) { setTop(pos); }

    inline iPointF topLeft() const { return iPointF(xp, yp); }
    inline iPointF bottomRight() const { return iPointF(xp+w, yp+h); }
    inline iPointF topRight() const { return iPointF(xp+w, yp); }
    inline iPointF bottomLeft() const { return iPointF(xp, yp+h); }
    inline iPointF center() const;

    inline void setTopLeft(const iPointF &p);
    inline void setBottomRight(const iPointF &p);
    inline void setTopRight(const iPointF &p);
    inline void setBottomLeft(const iPointF &p);

    inline void moveLeft(double pos);
    inline void moveTop(double pos);
    inline void moveRight(double pos);
    inline void moveBottom(double pos);
    inline void moveTopLeft(const iPointF &p);
    inline void moveBottomRight(const iPointF &p);
    inline void moveTopRight(const iPointF &p);
    inline void moveBottomLeft(const iPointF &p);
    inline void moveCenter(const iPointF &p);

    inline void translate(double dx, double dy);
    inline void translate(const iPointF &p);

    inline iRectF translated(double dx, double dy) const;
    inline iRectF translated(const iPointF &p) const;

    inline iRectF transposed() const;

    inline void moveTo(double x, double y);
    inline void moveTo(const iPointF &p);

    inline void setRect(double x, double y, double w, double h);
    inline void getRect(double *x, double *y, double *w, double *h) const;

    inline void setCoords(double x1, double y1, double x2, double y2);
    inline void getCoords(double *x1, double *y1, double *x2, double *y2) const;

    inline void adjust(double x1, double y1, double x2, double y2);
    inline iRectF adjusted(double x1, double y1, double x2, double y2) const;

    inline iSizeF size() const;
    inline double width() const;
    inline double height() const;
    inline void setWidth(double w);
    inline void setHeight(double h);
    inline void setSize(const iSizeF &s);

    iRectF operator|(const iRectF &r) const;
    iRectF operator&(const iRectF &r) const;
    inline iRectF& operator|=(const iRectF &r);
    inline iRectF& operator&=(const iRectF &r);

    bool contains(const iRectF &r) const;
    bool contains(const iPointF &p) const;
    inline bool contains(double x, double y) const;
    inline iRectF united(const iRectF &other) const;
    inline iRectF intersected(const iRectF &other) const;
    bool intersects(const iRectF &r) const;

    iRectF unite(const iRectF &r) const { return united(r); }
    iRectF intersect(const iRectF &r) const { return intersected(r); }

    friend inline bool operator==(const iRectF &, const iRectF &);
    friend inline bool operator!=(const iRectF &, const iRectF &);

    inline iRect toRect() const;
    iRect toAlignedRect() const;

private:
    double xp;
    double yp;
    double w;
    double h;
};

inline bool operator==(const iRectF &, const iRectF &);
inline bool operator!=(const iRectF &, const iRectF &);

/*****************************************************************************
  iRectF inline member functions
 *****************************************************************************/

inline iRectF::iRectF(double aleft, double atop, double awidth, double aheight)
    : xp(aleft), yp(atop), w(awidth), h(aheight)
{
}

inline iRectF::iRectF(const iPointF &atopLeft, const iSizeF &asize)
    : xp(atopLeft.x()), yp(atopLeft.y()), w(asize.width()), h(asize.height())
{
}


inline iRectF::iRectF(const iPointF &atopLeft, const iPointF &abottomRight)
    : xp(atopLeft.x()), yp(atopLeft.y()), w(abottomRight.x() - atopLeft.x()), h(abottomRight.y() - atopLeft.y())
{
}

inline iRectF::iRectF(const iRect &r)
    : xp(r.x()), yp(r.y()), w(r.width()), h(r.height())
{
}

inline bool iRectF::isNull() const
{ return w == 0. && h == 0.; }

inline bool iRectF::isEmpty() const
{ return w <= 0. || h <= 0.; }

inline bool iRectF::isValid() const
{ return w > 0. && h > 0.; }

inline double iRectF::x() const
{ return xp; }

inline double iRectF::y() const
{ return yp; }

inline void iRectF::setLeft(double pos)
{ double diff = pos - xp; xp += diff; w -= diff; }

inline void iRectF::setRight(double pos)
{ w = pos - xp; }

inline void iRectF::setTop(double pos)
{ double diff = pos - yp; yp += diff; h -= diff; }

inline void iRectF::setBottom(double pos)
{ h = pos - yp; }

inline void iRectF::setTopLeft(const iPointF &p)
{ setLeft(p.x()); setTop(p.y()); }

inline void iRectF::setTopRight(const iPointF &p)
{ setRight(p.x()); setTop(p.y()); }

inline void iRectF::setBottomLeft(const iPointF &p)
{ setLeft(p.x()); setBottom(p.y()); }

inline void iRectF::setBottomRight(const iPointF &p)
{ setRight(p.x()); setBottom(p.y()); }

inline iPointF iRectF::center() const
{ return iPointF(xp + w/2, yp + h/2); }

inline void iRectF::moveLeft(double pos)
{ xp = pos; }

inline void iRectF::moveTop(double pos)
{ yp = pos; }

inline void iRectF::moveRight(double pos)
{ xp = pos - w; }

inline void iRectF::moveBottom(double pos)
{ yp = pos - h; }

inline void iRectF::moveTopLeft(const iPointF &p)
{ moveLeft(p.x()); moveTop(p.y()); }

inline void iRectF::moveTopRight(const iPointF &p)
{ moveRight(p.x()); moveTop(p.y()); }

inline void iRectF::moveBottomLeft(const iPointF &p)
{ moveLeft(p.x()); moveBottom(p.y()); }

inline void iRectF::moveBottomRight(const iPointF &p)
{ moveRight(p.x()); moveBottom(p.y()); }

inline void iRectF::moveCenter(const iPointF &p)
{ xp = p.x() - w/2; yp = p.y() - h/2; }

inline double iRectF::width() const
{ return w; }

inline double iRectF::height() const
{ return h; }

inline iSizeF iRectF::size() const
{ return iSizeF(w, h); }

inline void iRectF::translate(double dx, double dy)
{
    xp += dx;
    yp += dy;
}

inline void iRectF::translate(const iPointF &p)
{
    xp += p.x();
    yp += p.y();
}

inline void iRectF::moveTo(double ax, double ay)
{
    xp = ax;
    yp = ay;
}

inline void iRectF::moveTo(const iPointF &p)
{
    xp = p.x();
    yp = p.y();
}

inline iRectF iRectF::translated(double dx, double dy) const
{ return iRectF(xp + dx, yp + dy, w, h); }

inline iRectF iRectF::translated(const iPointF &p) const
{ return iRectF(xp + p.x(), yp + p.y(), w, h); }

inline iRectF iRectF::transposed() const
{ return iRectF(topLeft(), size().transposed()); }

inline void iRectF::getRect(double *ax, double *ay, double *aaw, double *aah) const
{
    *ax = this->xp;
    *ay = this->yp;
    *aaw = this->w;
    *aah = this->h;
}

inline void iRectF::setRect(double ax, double ay, double aaw, double aah)
{
    this->xp = ax;
    this->yp = ay;
    this->w = aaw;
    this->h = aah;
}

inline void iRectF::getCoords(double *xp1, double *yp1, double *xp2, double *yp2) const
{
    *xp1 = xp;
    *yp1 = yp;
    *xp2 = xp + w;
    *yp2 = yp + h;
}

inline void iRectF::setCoords(double xp1, double yp1, double xp2, double yp2)
{
    xp = xp1;
    yp = yp1;
    w = xp2 - xp1;
    h = yp2 - yp1;
}

inline void iRectF::adjust(double xp1, double yp1, double xp2, double yp2)
{ xp += xp1; yp += yp1; w += xp2 - xp1; h += yp2 - yp1; }

inline iRectF iRectF::adjusted(double xp1, double yp1, double xp2, double yp2) const
{ return iRectF(xp + xp1, yp + yp1, w + xp2 - xp1, h + yp2 - yp1); }

inline void iRectF::setWidth(double aw)
{ this->w = aw; }

inline void iRectF::setHeight(double ah)
{ this->h = ah; }

inline void iRectF::setSize(const iSizeF &s)
{
    w = s.width();
    h = s.height();
}

inline bool iRectF::contains(double ax, double ay) const
{
    return contains(iPointF(ax, ay));
}

inline iRectF& iRectF::operator|=(const iRectF &r)
{
    *this = *this | r;
    return *this;
}

inline iRectF& iRectF::operator&=(const iRectF &r)
{
    *this = *this & r;
    return *this;
}

inline iRectF iRectF::intersected(const iRectF &r) const
{
    return *this & r;
}

inline iRectF iRectF::united(const iRectF &r) const
{
    return *this | r;
}

inline bool operator==(const iRectF &r1, const iRectF &r2)
{
    return iFuzzyCompare(r1.xp, r2.xp) && iFuzzyCompare(r1.yp, r2.yp)
           && iFuzzyCompare(r1.w, r2.w) && iFuzzyCompare(r1.h, r2.h);
}

inline bool operator!=(const iRectF &r1, const iRectF &r2)
{
    return !iFuzzyCompare(r1.xp, r2.xp) || !iFuzzyCompare(r1.yp, r2.yp)
           || !iFuzzyCompare(r1.w, r2.w) || !iFuzzyCompare(r1.h, r2.h);
}

inline iRect iRectF::toRect() const
{
    return iRect(std::round(xp), std::round(yp), std::round(w), std::round(h));
}

} // namespace iShell

#endif // IRECT_H
