/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    istringiterator_p.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef ISTRINGITERATOR_P_H
#define ISTRINGITERATOR_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the iShell API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <core/global/imacro.h>
#include <core/global/iglobal.h>
#include <core/utils/istring.h>

namespace iShell {

class iStringIterator
{
    iString::const_iterator i, pos, e;
    IX_COMPILER_VERIFY((std::is_same<iString::const_iterator, const iChar *>::value));
public:
    explicit iStringIterator(iStringView string, xsizetype idx = 0)
        : i(string.begin()),
          pos(i + idx),
          e(string.end())
    {
    }

    inline explicit iStringIterator(const iChar *begin, const iChar *end)
        : i(begin),
          pos(begin),
          e(end)
    {
    }

    inline explicit iStringIterator(const iChar *begin, int idx, const iChar *end)
        : i(begin),
          pos(begin + idx),
          e(end)
    {
    }

    inline iString::const_iterator position() const
    {
        return pos;
    }

    inline int index() const
    {
        return pos - i;
    }

    inline void setPosition(iString::const_iterator position)
    {
        IX_ASSERT_X(i <= position && position <= e, "position out of bounds");
        pos = position;
    }

    // forward iteration

    inline bool hasNext() const
    {
        return pos < e;
    }

    inline void advance()
    {
        IX_ASSERT_X(hasNext(), "iterator hasn't a next item");

        if (((pos++)->isHighSurrogate())) {
            if ((pos != e && pos->isLowSurrogate()))
                ++pos;
        }
    }

    inline void advanceUnchecked()
    {
        IX_ASSERT_X(hasNext(), "iterator hasn't a next item");

        if (((pos++)->isHighSurrogate()))
            ++pos;
    }

    inline uint peekNextUnchecked() const
    {
        IX_ASSERT_X(hasNext(), "iterator hasn't a next item");

        if ((pos->isHighSurrogate()))
            return iChar::surrogateToUcs4(pos[0], pos[1]);

        return pos->unicode();
    }

    inline uint peekNext(uint invalidAs = iChar::ReplacementCharacter) const
    {
        IX_ASSERT_X(hasNext(), "iterator hasn't a next item");

        if ((pos->isSurrogate())) {
            if ((pos->isHighSurrogate())) {
                const iChar *low = pos + 1;
                if ((low != e && low->isLowSurrogate()))
                    return iChar::surrogateToUcs4(*pos, *low);
            }
            return invalidAs;
        }

        return pos->unicode();
    }

    inline uint nextUnchecked()
    {
        IX_ASSERT_X(hasNext(), "iterator hasn't a next item");

        const iChar cur = *pos++;
        if ((cur.isHighSurrogate()))
            return iChar::surrogateToUcs4(cur, *pos++);
        return cur.unicode();
    }

    inline uint next(uint invalidAs = iChar::ReplacementCharacter)
    {
        IX_ASSERT_X(hasNext(), "iterator hasn't a next item");

        const iChar uc = *pos++;
        if ((uc.isSurrogate())) {
            if ((uc.isHighSurrogate() && pos < e && pos->isLowSurrogate()))
                return iChar::surrogateToUcs4(uc, *pos++);
            return invalidAs;
        }

        return uc.unicode();
    }

    // backwards iteration

    inline bool hasPrevious() const
    {
        return pos > i;
    }

    inline void recede()
    {
        IX_ASSERT_X(hasPrevious(), "iterator hasn't a previous item");

        if (((--pos)->isLowSurrogate())) {
            const iChar *high = pos - 1;
            if ((high != i - 1 && high->isHighSurrogate()))
                --pos;
        }
    }

    inline void recedeUnchecked()
    {
        IX_ASSERT_X(hasPrevious(), "iterator hasn't a previous item");

        if (((--pos)->isLowSurrogate()))
            --pos;
    }

    inline uint peekPreviousUnchecked() const
    {
        IX_ASSERT_X(hasPrevious(), "iterator hasn't a previous item");

        if ((pos[-1].isLowSurrogate()))
            return iChar::surrogateToUcs4(pos[-2], pos[-1]);
        return pos[-1].unicode();
    }

    inline uint peekPrevious(uint invalidAs = iChar::ReplacementCharacter) const
    {
        IX_ASSERT_X(hasPrevious(), "iterator hasn't a previous item");

        if ((pos[-1].isSurrogate())) {
            if ((pos[-1].isLowSurrogate())) {
                const iChar *high = pos - 2;
                if ((high != i - 1 && high->isHighSurrogate()))
                    return iChar::surrogateToUcs4(*high, pos[-1]);
            }
            return invalidAs;
        }

        return pos[-1].unicode();
    }

    inline uint previousUnchecked()
    {
        IX_ASSERT_X(hasPrevious(), "iterator hasn't a previous item");

        const iChar cur = *--pos;
        if ((cur.isLowSurrogate()))
            return iChar::surrogateToUcs4(*--pos, cur);
        return cur.unicode();
    }

    inline uint previous(uint invalidAs = iChar::ReplacementCharacter)
    {
        IX_ASSERT_X(hasPrevious(), "iterator hasn't a previous item");

        const iChar uc = *--pos;
        if ((uc.isSurrogate())) {
            if ((uc.isLowSurrogate() && pos > i && pos[-1].isHighSurrogate()))
                return iChar::surrogateToUcs4(*--pos, uc);
            return invalidAs;
        }

        return uc.unicode();
    }
};

} // namespace iShell

#endif // ISTRINGITERATOR_P_H
