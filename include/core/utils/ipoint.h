/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ipoint.h
/// @brief   represent 2D points with integer and floating-point coordinates
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IPOINT_H
#define IPOINT_H

#include <core/global/inumeric.h>

namespace iShell {

class IX_CORE_EXPORT iPoint
{
public:
    iPoint();
    iPoint(int xpos, int ypos);

    inline bool isNull() const;

    inline int x() const;
    inline int y() const;
    inline void setX(int x);
    inline void setY(int y);

    inline int manhattanLength() const;

    inline int &rx();
    inline int &ry();

    inline iPoint &operator+=(const iPoint &p);
    inline iPoint &operator-=(const iPoint &p);

    inline iPoint &operator*=(float factor);
    inline iPoint &operator*=(double factor);
    inline iPoint &operator*=(int factor);

    inline iPoint &operator/=(double divisor);

    static inline int dotProduct(const iPoint &p1, const iPoint &p2)
    { return p1.xp * p2.xp + p1.yp * p2.yp; }

    friend inline bool operator==(const iPoint &, const iPoint &);
    friend inline bool operator!=(const iPoint &, const iPoint &);
    friend inline const iPoint operator+(const iPoint &, const iPoint &);
    friend inline const iPoint operator-(const iPoint &, const iPoint &);
    friend inline const iPoint operator*(const iPoint &, float);
    friend inline const iPoint operator*(float, const iPoint &);
    friend inline const iPoint operator*(const iPoint &, double);
    friend inline const iPoint operator*(double, const iPoint &);
    friend inline const iPoint operator*(const iPoint &, int);
    friend inline const iPoint operator*(int, const iPoint &);
    friend inline const iPoint operator+(const iPoint &);
    friend inline const iPoint operator-(const iPoint &);
    friend inline const iPoint operator/(const iPoint &, double);

private:
    int xp;
    int yp;
};

/*****************************************************************************
  iPoint inline functions
 *****************************************************************************/

inline iPoint::iPoint() : xp(0), yp(0) {}

inline iPoint::iPoint(int xpos, int ypos) : xp(xpos), yp(ypos) {}

inline bool iPoint::isNull() const
{ return xp == 0 && yp == 0; }

inline int iPoint::x() const
{ return xp; }

inline int iPoint::y() const
{ return yp; }

inline void iPoint::setX(int xpos)
{ xp = xpos; }

inline void iPoint::setY(int ypos)
{ yp = ypos; }

inline int iPoint::manhattanLength() const
{ return std::abs(x())+std::abs(y()); }

inline int &iPoint::rx()
{ return xp; }

inline int &iPoint::ry()
{ return yp; }

inline iPoint &iPoint::operator+=(const iPoint &p)
{ xp+=p.xp; yp+=p.yp; return *this; }

inline iPoint &iPoint::operator-=(const iPoint &p)
{ xp-=p.xp; yp-=p.yp; return *this; }

inline iPoint &iPoint::operator*=(float factor)
{ xp = int(std::round(xp*factor)); yp = int(std::round(yp*factor)); return *this; }

inline iPoint &iPoint::operator*=(double factor)
{ xp = int(std::round(xp*factor)); yp = int(std::round(yp*factor)); return *this; }

inline iPoint &iPoint::operator*=(int factor)
{ xp = xp*factor; yp = yp*factor; return *this; }

inline bool operator==(const iPoint &p1, const iPoint &p2)
{ return p1.xp == p2.xp && p1.yp == p2.yp; }

inline bool operator!=(const iPoint &p1, const iPoint &p2)
{ return p1.xp != p2.xp || p1.yp != p2.yp; }

inline const iPoint operator+(const iPoint &p1, const iPoint &p2)
{ return iPoint(p1.xp+p2.xp, p1.yp+p2.yp); }

inline const iPoint operator-(const iPoint &p1, const iPoint &p2)
{ return iPoint(p1.xp-p2.xp, p1.yp-p2.yp); }

inline const iPoint operator*(const iPoint &p, float factor)
{ return iPoint(int(std::round(p.xp*factor)), int(std::round(p.yp*factor))); }

inline const iPoint operator*(const iPoint &p, double factor)
{ return iPoint(int(std::round(p.xp*factor)), int(std::round(p.yp*factor))); }

inline const iPoint operator*(const iPoint &p, int factor)
{ return iPoint(p.xp*factor, p.yp*factor); }

inline const iPoint operator*(float factor, const iPoint &p)
{ return iPoint(int(std::round(p.xp*factor)), int(std::round(p.yp*factor))); }

inline const iPoint operator*(double factor, const iPoint &p)
{ return iPoint(int(std::round(p.xp*factor)), int(std::round(p.yp*factor))); }

inline const iPoint operator*(int factor, const iPoint &p)
{ return iPoint(p.xp*factor, p.yp*factor); }

inline const iPoint operator+(const iPoint &p)
{ return p; }

inline const iPoint operator-(const iPoint &p)
{ return iPoint(-p.xp, -p.yp); }

inline iPoint &iPoint::operator/=(double c)
{
    xp = int(std::round(xp/c));
    yp = int(std::round(yp/c));
    return *this;
}

inline const iPoint operator/(const iPoint &p, double c)
{
    return iPoint(int(std::round(p.xp/c)), int(std::round(p.yp/c)));
}

class IX_CORE_EXPORT iPointF
{
public:
    iPointF();
    iPointF(const iPoint &p);
    iPointF(double xpos, double ypos);

    inline double manhattanLength() const;

    inline bool isNull() const;

    inline double x() const;
    inline double y() const;
    inline void setX(double x);
    inline void setY(double y);

    inline double &rx();
    inline double &ry();

    inline iPointF &operator+=(const iPointF &p);
    inline iPointF &operator-=(const iPointF &p);
    inline iPointF &operator*=(double c);
    inline iPointF &operator/=(double c);

    static inline double dotProduct(const iPointF &p1, const iPointF &p2)
    { return p1.xp * p2.xp + p1.yp * p2.yp; }

    friend inline bool operator==(const iPointF &, const iPointF &);
    friend inline bool operator!=(const iPointF &, const iPointF &);
    friend inline const iPointF operator+(const iPointF &, const iPointF &);
    friend inline const iPointF operator-(const iPointF &, const iPointF &);
    friend inline const iPointF operator*(double, const iPointF &);
    friend inline const iPointF operator*(const iPointF &, double);
    friend inline const iPointF operator+(const iPointF &);
    friend inline const iPointF operator-(const iPointF &);
    friend inline const iPointF operator/(const iPointF &, double);

    iPoint toPoint() const;

private:
    double xp;
    double yp;
};

/*****************************************************************************
  iPointF inline functions
 *****************************************************************************/

inline iPointF::iPointF() : xp(0), yp(0) {}

inline iPointF::iPointF(double xpos, double ypos) : xp(xpos), yp(ypos) {}

inline iPointF::iPointF(const iPoint &p) : xp(p.x()), yp(p.y()) {}

inline double iPointF::manhattanLength() const
{ return std::abs(x())+std::abs(y()); }

inline bool iPointF::isNull() const
{ return iIsNull(xp) && iIsNull(yp); }

inline double iPointF::x() const
{ return xp; }

inline double iPointF::y() const
{ return yp; }

inline void iPointF::setX(double xpos)
{ xp = xpos; }

inline void iPointF::setY(double ypos)
{ yp = ypos; }

inline double &iPointF::rx()
{ return xp; }

inline double &iPointF::ry()
{ return yp; }

inline iPointF &iPointF::operator+=(const iPointF &p)
{
    xp+=p.xp;
    yp+=p.yp;
    return *this;
}

inline iPointF &iPointF::operator-=(const iPointF &p)
{ xp-=p.xp; yp-=p.yp; return *this; }

inline iPointF &iPointF::operator*=(double c)
{ xp*=c; yp*=c; return *this; }

inline bool operator==(const iPointF &p1, const iPointF &p2)
{ return iFuzzyIsNull(p1.xp - p2.xp) && iFuzzyIsNull(p1.yp - p2.yp); }

inline bool operator!=(const iPointF &p1, const iPointF &p2)
{ return !iFuzzyIsNull(p1.xp - p2.xp) || !iFuzzyIsNull(p1.yp - p2.yp); }

inline const iPointF operator+(const iPointF &p1, const iPointF &p2)
{ return iPointF(p1.xp+p2.xp, p1.yp+p2.yp); }

inline const iPointF operator-(const iPointF &p1, const iPointF &p2)
{ return iPointF(p1.xp-p2.xp, p1.yp-p2.yp); }

inline const iPointF operator*(const iPointF &p, double c)
{ return iPointF(p.xp*c, p.yp*c); }

inline const iPointF operator*(double c, const iPointF &p)
{ return iPointF(p.xp*c, p.yp*c); }

inline const iPointF operator+(const iPointF &p)
{ return p; }

inline const iPointF operator-(const iPointF &p)
{ return iPointF(-p.xp, -p.yp); }

inline iPointF &iPointF::operator/=(double divisor)
{
    xp/=divisor;
    yp/=divisor;
    return *this;
}

inline const iPointF operator/(const iPointF &p, double divisor)
{ return iPointF(p.xp/divisor, p.yp/divisor); }

inline iPoint iPointF::toPoint() const
{ return iPoint(int(std::round(xp)), int(std::round(yp))); }

} // namespace iShell

#endif // IPOINT_H
