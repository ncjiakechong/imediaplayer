/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    ibasicatomicbitfield.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IBASICATOMICBITFIELD_H
#define IBASICATOMICBITFIELD_H

#include <limits>
#include <core/thread/iatomiccounter.h>

namespace iShell {

template <size_t N>
struct iBasicAtomicBitField {
    enum {
        BitsPerInt = std::numeric_limits<uint>::digits,
        NumInts = (N + BitsPerInt - 1) / BitsPerInt,
        NumBits = N
    };

    // This atomic int points to the next (possibly) free ID saving
    // the otherwise necessary scan through 'data':
    iAtomicCounter<int> next;
    iAtomicCounter<int> data[NumInts];

    bool allocateSpecific(int which)
    {
        iAtomicCounter<int>& entry = data[which / BitsPerInt];
        const uint old = entry.value();
        const uint bit = 1U << (which % BitsPerInt);
        return !(old & bit) // wasn't taken
            && entry.testAndSet(old, old | bit); // still wasn't taken

        // don't update 'next' here - it's unlikely that it will need
        // to be updated, in the general case, and having 'next'
        // trailing a bit is not a problem, as it is just a starting
        // hint for allocateNext(), which, when wrong, will just
        // result in a few more rounds through the allocateNext()
        // loop.
    }

    int allocateNext()
    {
        // Unroll loop to iterate over ints, then bits? Would save
        // potentially a lot of cmpxchgs, because we can scan the
        // whole int before having to load it again.

        // Then again, this should never execute many iterations, so
        // leave like this for now:
        for (uint i = next.value(); i < NumBits; ++i) {
            if (allocateSpecific(i)) {
                // remember next (possibly) free id:
                const uint oldNext = next.value();
                next.testAndSet(oldNext, std::max(i + 1, oldNext));
                return i;
            }
        }
        return -1;
    }
};

} // namespace iShell
#endif // IBASICATOMICBITFIELD_H
