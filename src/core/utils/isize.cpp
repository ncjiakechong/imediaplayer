/////////////////////////////////////////////////////////////////
/// Copyright 2012-2018
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    isize.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/// @date    2018-12-27
/////////////////////////////////////////////////////////////////
/// Edit History
/// -----------------------------------------------------------
/// DATE                     NAME          DESCRIPTION
/// 2018-12-27          anfengce@        Create.
/////////////////////////////////////////////////////////////////

#include "core/utils/isize.h"

namespace ishell {

void iSize::transpose()
{
    std::swap(wd, ht);
}

iSize iSize::scaled(const iSize &s, AspectRatioMode mode) const
{
    if (mode == IgnoreAspectRatio || wd == 0 || ht == 0) {
        return s;
    } else {
        bool useHeight;
        int64_t rw = int64_t(s.ht) * int64_t(wd) / int64_t(ht);

        if (mode == KeepAspectRatio) {
            useHeight = (rw <= s.wd);
        } else { // mode == KeepAspectRatioByExpanding
            useHeight = (rw >= s.wd);
        }

        if (useHeight) {
            return iSize(rw, s.ht);
        } else {
            return iSize(s.wd,
                         int32_t(int64_t(s.wd) * int64_t(ht) / int64_t(wd)));
        }
    }
}

void iSizeF::transpose()
{
    std::swap(wd, ht);
}

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
} // namespace ishell
