/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imemoryvideobuffer.cpp
/// @brief   provides a system memory allocated video data buffer
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "imemoryvideobuffer_p.h"
#include "iabstractvideobuffer_p.h"

namespace iShell {

/*!
    \class iMemoryVideoBuffer
    \brief The iMemoryVideoBuffer class provides a system memory allocated video data buffer.
    \internal

    iMemoryVideoBuffer is the default video buffer for allocating system memory.  It may be used to
    allocate memory for a iVideoFrame without implementing your own iAbstractVideoBuffer.
*/
iMemoryVideoBuffer::iMemoryVideoBuffer(const iByteArray &array, int bytesPerLine)
    : iAbstractVideoBuffer( NoHandle)
    , m_bytesPerLine(bytesPerLine)
    , m_mapMode(NotMapped)
    , m_data(array)
{
}

iMemoryVideoBuffer::~iMemoryVideoBuffer()
{
}

iAbstractVideoBuffer::MapMode iMemoryVideoBuffer::mapMode() const
{
    return m_mapMode;
}

uchar *iMemoryVideoBuffer::map(MapMode mode, int *numBytes, int *bytesPerLine)
{
    if (m_mapMode == NotMapped
        && m_data.data() && mode != NotMapped) {
        m_mapMode = mode;

        if (numBytes)
            *numBytes = m_data.size();

        if (bytesPerLine)
            *bytesPerLine = m_bytesPerLine;

        return reinterpret_cast<uchar *>(m_data.data());
    } else {
        return IX_NULLPTR;
    }
}

void iMemoryVideoBuffer::unmap()
{
    m_mapMode = NotMapped;
}

} // namespace iShell
