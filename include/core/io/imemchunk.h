/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imemchunk.h
/// @brief   provides tools for woking with segments of memory blocks
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IMEMCHUNK_H
#define IMEMCHUNK_H

#include <core/utils/ibytearray.h>

namespace iShell {

/* An alignment object, used for aligning memchunks to multiples of
 * the frame size. */

/**
 * Method of operation: the user creates a new mcalign object with the appropriate aligning
 * granularity. After that they may call push() for an input
 * memchunk. After exactly one memchunk the user has to call
 * pop() until it returns -1. If pop() returns
 * 0, the memchunk *c is valid and aligned to the granularity. 
 */
class IX_CORE_EXPORT iMCAlign
{
public:
    iMCAlign(size_t base);
    ~iMCAlign();

    /// If we pass l bytes in now, how many bytes would we get out?
    size_t csize(size_t l) const;

    /// Push a new memchunk into the aligner. The caller of this routine
    /// has to free the memchunk by himself.
    void push(const iByteArray& c);

    /// Pop a new memchunk from the aligner. Returns 0 when successful,
    /// nonzero otherwise.
    int pop(iByteArray& c);

    /// Flush what's still stored in the aligner
    void flush();

private:
    size_t    m_base;
    iByteArray m_leftover;
    iByteArray m_current;
};

} // namespace iShell

#endif // IMEMCHUNK_H
