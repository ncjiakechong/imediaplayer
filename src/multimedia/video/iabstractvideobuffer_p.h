/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iabstractvideobuffer_p.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IABSTRACTVIDEOBUFFER_P_H
#define IABSTRACTVIDEOBUFFER_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the public API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <multimedia/video/iabstractvideobuffer.h>

namespace iShell {

class iAbstractVideoBufferPrivate
{
public:
    iAbstractVideoBufferPrivate()
        : q_ptr(IX_NULLPTR)
    {}

    virtual ~iAbstractVideoBufferPrivate()
    {}

    virtual int map(
            iAbstractVideoBuffer::MapMode mode,
            int *numBytes,
            int bytesPerLine[4],
            uchar *data[4]);

    iAbstractVideoBuffer *q_ptr;
};

class iAbstractPlanarVideoBufferPrivate : iAbstractVideoBufferPrivate
{
public:
    iAbstractPlanarVideoBufferPrivate()
    {}

    int map(iAbstractVideoBuffer::MapMode mode, int *numBytes, int bytesPerLine[4], uchar *data[4]) override;

private:
    friend class iAbstractPlanarVideoBuffer;
};

} // namespace iShell


#endif // IABSTRACTVIDEOBUFFER_P_H
