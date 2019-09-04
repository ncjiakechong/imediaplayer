/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imemoryvideobuffer.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////

#include "imemoryvideobuffer_p.h"
#include "iabstractvideobuffer_p.h"

namespace iShell {

class iMemoryVideoBufferPrivate : public iAbstractVideoBufferPrivate
{
public:
    iMemoryVideoBufferPrivate()
        : bytesPerLine(0)
        , mapMode(iAbstractVideoBuffer::NotMapped)
    {
    }

    int bytesPerLine;
    iAbstractVideoBuffer::MapMode mapMode;
    iByteArray data;
};

/*!
    \class iMemoryVideoBuffer
    \brief The iMemoryVideoBuffer class provides a system memory allocated video data buffer.
    \internal

    iMemoryVideoBuffer is the default video buffer for allocating system memory.  It may be used to
    allocate memory for a iVideoFrame without implementing your own iAbstractVideoBuffer.
*/

/*!
    Constructs a video buffer with an image stride of \a bytesPerLine from a byte \a array.
*/
iMemoryVideoBuffer::iMemoryVideoBuffer(const iByteArray &array, int bytesPerLine)
    : iAbstractVideoBuffer(*new iMemoryVideoBufferPrivate, NoHandle)
{
    static_cast<iMemoryVideoBufferPrivate*>(d_ptr)->data = array;
    static_cast<iMemoryVideoBufferPrivate*>(d_ptr)->bytesPerLine = bytesPerLine;
}

/*!
    Destroys a system memory allocated video buffer.
*/
iMemoryVideoBuffer::~iMemoryVideoBuffer()
{
}

/*!
    \reimp
*/
iAbstractVideoBuffer::MapMode iMemoryVideoBuffer::mapMode() const
{
    return static_cast<iMemoryVideoBufferPrivate*>(d_ptr)->mapMode;
}

/*!
    \reimp
*/
uchar *iMemoryVideoBuffer::map(MapMode mode, int *numBytes, int *bytesPerLine)
{
    if (static_cast<iMemoryVideoBufferPrivate*>(d_ptr)->mapMode == NotMapped
        && static_cast<iMemoryVideoBufferPrivate*>(d_ptr)->data.data() && mode != NotMapped) {
        static_cast<iMemoryVideoBufferPrivate*>(d_ptr)->mapMode = mode;

        if (numBytes)
            *numBytes = static_cast<iMemoryVideoBufferPrivate*>(d_ptr)->data.size();

        if (bytesPerLine)
            *bytesPerLine = static_cast<iMemoryVideoBufferPrivate*>(d_ptr)->bytesPerLine;

        return reinterpret_cast<uchar *>(static_cast<iMemoryVideoBufferPrivate*>(d_ptr)->data.data());
    } else {
        return IX_NULLPTR;
    }
}

/*!
    \reimp
*/
void iMemoryVideoBuffer::unmap()
{
    static_cast<iMemoryVideoBufferPrivate*>(d_ptr)->mapMode = NotMapped;
}

} // namespace iShell
