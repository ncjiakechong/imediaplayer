/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    isize.cpp
/// @brief   represent 2D sizes (width and height) with integer and floating-point values
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "core/utils/isize.h"

namespace iShell {

void iSize::transpose()
{ std::swap(wd, ht); }

iSize iSize::scaled(const iSize &s, AspectRatioMode mode) const
{
    if (mode == IgnoreAspectRatio || wd == 0 || ht == 0) {
        return s;
    } else {
        bool useHeight;
        xint64 rw = xint64(s.ht) * xint64(wd) / xint64(ht);

        if (mode == KeepAspectRatio) {
            useHeight = (rw <= s.wd);
        } else { // mode == KeepAspectRatioByExpanding
            useHeight = (rw >= s.wd);
        }

        if (useHeight) {
            return iSize(rw, s.ht);
        } else {
            return iSize(s.wd,
                         xint32(xint64(s.wd) * xint64(ht) / xint64(wd)));
        }
    }
}

void iSizeF::transpose()
{ std::swap(wd, ht); }

iSizeF iSizeF::scaled(const iSizeF &s, AspectRatioMode mode) const
{
    if (mode == IgnoreAspectRatio || iIsNull(wd) || iIsNull(ht)) {
        return s;
    } else {
        bool useHeight;
        double rw = s.ht * wd / ht;

        if (mode == KeepAspectRatio) {
            useHeight = (rw <= s.wd);
        } else { // mode == KeepAspectRatioByExpanding
            useHeight = (rw >= s.wd);
        }

        if (useHeight) {
            return iSizeF(rw, s.ht);
        } else {
            return iSizeF(s.wd, s.wd * ht / wd);
        }
    }
}
} // namespace iShell
