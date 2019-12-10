/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    istringalgorithms_p.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef ISTRINGALGORITHMS_P_H
#define ISTRINGALGORITHMS_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the iShell API.  It exists for the convenience
// of internal files.  This header file may change from version to version
// without notice, or even be removed.
//
// We mean it.
//

#include <type_traits>

#include <core/utils/ichar.h>
#include <core/global/inamespace.h>
#include "utils/ilocale_tools_p.h"

namespace iShell {

template <typename StringType>
struct iStringAlgorithms
{
    typedef typename StringType::value_type Char;
    typedef typename StringType::size_type size_type;
    typedef typename std::remove_cv<StringType>::type NakedStringType;
    static const bool isConst = is_const<StringType>::value;


    static inline bool isSpace(char ch) { return ascii_isspace(ch); }
    static inline bool isSpace(iChar ch) { return ch.isSpace(); }

    // Surrogate pairs are not handled in either of the functions below. That is
    // not a problem because there are no space characters (Zs, Zl, Zp) outside the
    // Basic Multilingual Plane.

    static inline StringType trimmed_helper_inplace(NakedStringType &str, const Char *begin, const Char *end)
    {
        // in-place trimming:
        Char *data = const_cast<Char *>(str.cbegin());
        if (begin != data)
            memmove(data, begin, (end - begin) * sizeof(Char));
        str.resize(end - begin);
        return std::move(str);
    }

    static inline StringType trimmed_helper_inplace(const NakedStringType &, const Char *, const Char *)
    {
        // can't happen
        return StringType();
    }

    static inline void trimmed_helper_positions(const Char *&begin, const Char *&end)
    {
        // skip white space from end
        while (begin < end && isSpace(end[-1]))
            --end;
        // skip white space from start
        while (begin < end && isSpace(*begin))
            begin++;
    }

    static inline StringType trimmed_helper(StringType &str)
    {
        const Char *begin = str.cbegin();
        const Char *end = str.cend();
        trimmed_helper_positions(begin, end);

        if (begin == str.cbegin() && end == str.cend())
            return str;
        if (!isConst && str.isDetached())
            return trimmed_helper_inplace(str, begin, end);
        return StringType(begin, end - begin);
    }

    static inline StringType simplified_helper(StringType &str)
    {
        if (str.isEmpty())
            return str;
        const Char *src = str.cbegin();
        const Char *end = str.cend();
        NakedStringType result = isConst || !str.isDetached() ?
                                     StringType(str.size(), iShell::Uninitialized) :
                                     std::move(str);

        Char *dst = const_cast<Char *>(result.cbegin());
        Char *ptr = dst;
        bool unmodified = true;
        do {
            while (src != end && isSpace(*src))
                ++src;
            while (src != end && !isSpace(*src))
                *ptr++ = *src++;
            if (src == end)
                break;
            if (*src != iChar::Space)
                unmodified = false;
            *ptr++ = iChar::Space;
        } while (true);

        if (ptr != dst && ptr[-1] == iChar::Space)
            --ptr;

        int newlen = ptr - dst;
        if (isConst && newlen == str.size() && unmodified) {
            // nothing happened, return the original
            return str;
        }
        result.resize(newlen);
        return result;
    }
};

} // namespace iShell

#endif // ISTRINGALGORITHMS_P_H
