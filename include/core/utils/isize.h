/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    isize.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef ISIZE_H
#define ISIZE_H

#include <core/global/imacro.h>
#include <core/global/iglobal.h>
#include <core/global/inamespace.h>

namespace iShell {

class IX_CORE_EXPORT iSize
{
public:
    iSize();
    iSize(int w, int h);

    inline bool isNull() const;
    inline bool isEmpty() const;
    inline bool isValid() const;

    inline int width() const;
    inline int height() const;
    inline void setWidth(int w);
    inline void setHeight(int h);
    void transpose();
    inline iSize transposed() const;

    inline void scale(int w, int h, AspectRatioMode mode);
    inline void scale(const iSize &s, AspectRatioMode mode);
    iSize scaled(int w, int h, AspectRatioMode mode) const;
    iSize scaled(const iSize &s, AspectRatioMode mode) const;

    inline iSize expandedTo(const iSize &) const;
    inline iSize boundedTo(const iSize &) const;

    inline int &rwidth();
    inline int &rheight();

    inline iSize &operator+=(const iSize &);
    inline iSize &operator-=(const iSize &);
    inline iSize &operator*=(double c);
    inline iSize &operator/=(double c);

    friend inline bool operator==(const iSize &, const iSize &);
    friend inline bool operator!=(const iSize &, const iSize &);
    friend inline const iSize operator+(const iSize &, const iSize &);
    friend inline const iSize operator-(const iSize &, const iSize &);
    friend inline const iSize operator*(const iSize &, double);
    friend inline const iSize operator*(double, const iSize &);
    friend inline const iSize operator/(const iSize &, double);

private:
    int wd;
    int ht;
};

/*****************************************************************************
  iSize inline functions
 *****************************************************************************/

inline iSize::iSize() : wd(-1), ht(-1) {}

inline iSize::iSize(int w, int h) : wd(w), ht(h) {}

inline bool iSize::isNull() const
{ return wd==0 && ht==0; }

inline bool iSize::isEmpty() const
{ return wd<1 || ht<1; }

inline bool iSize::isValid() const
{ return wd>=0 && ht>=0; }

inline int iSize::width() const
{ return wd; }

inline int iSize::height() const
{ return ht; }

inline void iSize::setWidth(int w)
{ wd = w; }

inline void iSize::setHeight(int h)
{ ht = h; }

inline iSize iSize::transposed() const
{ return iSize(ht, wd); }

inline void iSize::scale(int w, int h, AspectRatioMode mode)
{ scale(iSize(w, h), mode); }

inline void iSize::scale(const iSize &s, AspectRatioMode mode)
{ *this = scaled(s, mode); }

inline iSize iSize::scaled(int w, int h, AspectRatioMode mode) const
{ return scaled(iSize(w, h), mode); }

inline int &iSize::rwidth()
{ return wd; }

inline int &iSize::rheight()
{ return ht; }

inline iSize &iSize::operator+=(const iSize &s)
{ wd+=s.wd; ht+=s.ht; return *this; }

inline iSize &iSize::operator-=(const iSize &s)
{ wd-=s.wd; ht-=s.ht; return *this; }

inline iSize &iSize::operator*=(double c)
{ wd = int(std::round(wd*c)); ht = int(std::round(ht*c)); return *this; }

inline bool operator==(const iSize &s1, const iSize &s2)
{ return s1.wd == s2.wd && s1.ht == s2.ht; }

inline bool operator!=(const iSize &s1, const iSize &s2)
{ return s1.wd != s2.wd || s1.ht != s2.ht; }

inline const iSize operator+(const iSize & s1, const iSize & s2)
{ return iSize(s1.wd+s2.wd, s1.ht+s2.ht); }

inline const iSize operator-(const iSize &s1, const iSize &s2)
{ return iSize(s1.wd-s2.wd, s1.ht-s2.ht); }

inline const iSize operator*(const iSize &s, double c)
{ return iSize(int(std::round(s.wd*c)), int(std::round(s.ht*c))); }

inline const iSize operator*(double c, const iSize &s)
{ return iSize(int(std::round(s.wd*c)), int(std::round(s.ht*c))); }

inline iSize &iSize::operator/=(double c)
{
    IX_ASSERT(!iFuzzyIsNull(c));
    wd = int(std::round(wd/c)); ht = int(std::round(ht/c));
    return *this;
}

inline const iSize operator/(const iSize &s, double c)
{
    IX_ASSERT(!iFuzzyIsNull(c));
    return iSize(int(std::round(s.wd/c)), int(std::round(s.ht/c)));
}

inline iSize iSize::expandedTo(const iSize & otherSize) const
{
    return iSize(std::max(wd,otherSize.wd), std::max(ht,otherSize.ht));
}

inline iSize iSize::boundedTo(const iSize & otherSize) const
{
    return iSize(std::min(wd,otherSize.wd), std::min(ht,otherSize.ht));
}

class IX_CORE_EXPORT iSizeF
{
public:
    iSizeF();
    iSizeF(const iSize &sz);
    iSizeF(double w, double h);

    inline bool isNull() const;
    inline bool isEmpty() const;
    inline bool isValid() const;

    inline double width() const;
    inline double height() const;
    inline void setWidth(double w);
    inline void setHeight(double h);
    void transpose();
    inline iSizeF transposed() const;

    inline void scale(double w, double h, AspectRatioMode mode);
    inline void scale(const iSizeF &s, AspectRatioMode mode);
    iSizeF scaled(double w, double h, AspectRatioMode mode) const;
    iSizeF scaled(const iSizeF &s, AspectRatioMode mode) const;

    inline iSizeF expandedTo(const iSizeF &) const;
    inline iSizeF boundedTo(const iSizeF &) const;

    inline double &rwidth();
    inline double &rheight();

    inline iSizeF &operator+=(const iSizeF &);
    inline iSizeF &operator-=(const iSizeF &);
    inline iSizeF &operator*=(double c);
    inline iSizeF &operator/=(double c);

    friend inline bool operator==(const iSizeF &, const iSizeF &);
    friend inline bool operator!=(const iSizeF &, const iSizeF &);
    friend inline const iSizeF operator+(const iSizeF &, const iSizeF &);
    friend inline const iSizeF operator-(const iSizeF &, const iSizeF &);
    friend inline const iSizeF operator*(const iSizeF &, double);
    friend inline const iSizeF operator*(double, const iSizeF &);
    friend inline const iSizeF operator/(const iSizeF &, double);

    inline iSize toSize() const;

private:
    double wd;
    double ht;
};

/*****************************************************************************
  iSizeF inline functions
 *****************************************************************************/

inline iSizeF::iSizeF() : wd(-1.), ht(-1.) {}

inline iSizeF::iSizeF(const iSize &sz) : wd(sz.width()), ht(sz.height()) {}

inline iSizeF::iSizeF(double w, double h) : wd(w), ht(h) {}

inline bool iSizeF::isNull() const
{ return iIsNull(wd) && iIsNull(ht); }

inline bool iSizeF::isEmpty() const
{ return wd <= 0. || ht <= 0.; }

inline bool iSizeF::isValid() const
{ return wd >= 0. && ht >= 0.; }

inline double iSizeF::width() const
{ return wd; }

inline double iSizeF::height() const
{ return ht; }

inline void iSizeF::setWidth(double w)
{ wd = w; }

inline void iSizeF::setHeight(double h)
{ ht = h; }

inline iSizeF iSizeF::transposed() const
{ return iSizeF(ht, wd); }

inline void iSizeF::scale(double w, double h, AspectRatioMode mode)
{ scale(iSizeF(w, h), mode); }

inline void iSizeF::scale(const iSizeF &s, AspectRatioMode mode)
{ *this = scaled(s, mode); }

inline iSizeF iSizeF::scaled(double w, double h, AspectRatioMode mode) const
{ return scaled(iSizeF(w, h), mode); }

inline double &iSizeF::rwidth()
{ return wd; }

inline double &iSizeF::rheight()
{ return ht; }

inline iSizeF &iSizeF::operator+=(const iSizeF &s)
{ wd += s.wd; ht += s.ht; return *this; }

inline iSizeF &iSizeF::operator-=(const iSizeF &s)
{ wd -= s.wd; ht -= s.ht; return *this; }

inline iSizeF &iSizeF::operator*=(double c)
{ wd *= c; ht *= c; return *this; }

inline bool operator==(const iSizeF &s1, const iSizeF &s2)
{ return iFuzzyCompare(s1.wd, s2.wd) && iFuzzyCompare(s1.ht, s2.ht); }

inline bool operator!=(const iSizeF &s1, const iSizeF &s2)
{ return !iFuzzyCompare(s1.wd, s2.wd) || !iFuzzyCompare(s1.ht, s2.ht); }

inline const iSizeF operator+(const iSizeF & s1, const iSizeF & s2)
{ return iSizeF(s1.wd+s2.wd, s1.ht+s2.ht); }

inline const iSizeF operator-(const iSizeF &s1, const iSizeF &s2)
{ return iSizeF(s1.wd-s2.wd, s1.ht-s2.ht); }

inline const iSizeF operator*(const iSizeF &s, double c)
{ return iSizeF(s.wd*c, s.ht*c); }

inline const iSizeF operator*(double c, const iSizeF &s)
{ return iSizeF(s.wd*c, s.ht*c); }

inline iSizeF &iSizeF::operator/=(double c)
{
    IX_ASSERT(!iFuzzyIsNull(c));
    wd = wd/c; ht = ht/c;
    return *this;
}

inline const iSizeF operator/(const iSizeF &s, double c)
{
    IX_ASSERT(!iFuzzyIsNull(c));
    return iSizeF(s.wd/c, s.ht/c);
}

inline iSizeF iSizeF::expandedTo(const iSizeF & otherSize) const
{
    return iSizeF(std::max(wd,otherSize.wd), std::max(ht,otherSize.ht));
}

inline iSizeF iSizeF::boundedTo(const iSizeF & otherSize) const
{
    return iSizeF(std::min(wd,otherSize.wd), std::min(ht,otherSize.ht));
}

inline iSize iSizeF::toSize() const
{
    return iSize(int(std::round(wd)), int(std::round(ht)));
}

} // namespace iShell

#endif // ISIZE_H
