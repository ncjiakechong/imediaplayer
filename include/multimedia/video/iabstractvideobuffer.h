/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iabstractvideobuffer.h
/// @brief   provide an abstraction for working with video buffers, 
///          allowing access to the underlying video data for rendering or processing
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IABSTRACTVIDEOBUFFER_H
#define IABSTRACTVIDEOBUFFER_H

#include <core/global/iglobal.h>
#include <core/global/imacro.h>

#include <multimedia/imultimediaglobal.h>

namespace iShell {

class iVariant;

class IX_MULTIMEDIA_EXPORT iAbstractVideoBuffer
{
public:
    enum HandleType
    {
        NoHandle,
        GLTextureHandle,
        XvShmImageHandle,
        CoreImageHandle,
        IPixmapHandle,
        EGLImageHandle,
        UserHandle = 1000
    };

    enum MapMode
    {
        NotMapped = 0x00,
        ReadOnly  = 0x01,
        WriteOnly = 0x02,
        ReadWrite = ReadOnly | WriteOnly
    };

    iAbstractVideoBuffer(HandleType type);
    virtual ~iAbstractVideoBuffer();
    virtual void release();

    HandleType handleType() const;

    virtual MapMode mapMode() const = 0;

    virtual uchar *map(MapMode mode, int *numBytes, int *bytesPerLine) = 0;
    int mapPlanes(MapMode mode, int *numBytes, int bytesPerLine[4], uchar *data[4]);
    virtual void unmap() = 0;

    virtual iVariant handle() const;

protected:
    virtual int mapImpl(MapMode mode, int *numBytes, int bytesPerLine[4], uchar *data[4]);

    HandleType m_type;

private:
    IX_DISABLE_COPY(iAbstractVideoBuffer)
};

class IX_MULTIMEDIA_EXPORT iAbstractPlanarVideoBuffer : public iAbstractVideoBuffer
{
public:
    iAbstractPlanarVideoBuffer(HandleType type);
    virtual ~iAbstractPlanarVideoBuffer() override;

    uchar *map(MapMode mode, int *numBytes, int *bytesPerLine) override;
    virtual int map(MapMode mode, int *numBytes, int bytesPerLine[4], uchar *data[4]) = 0;

protected:
    virtual int mapImpl(MapMode mode, int *numBytes, int bytesPerLine[4], uchar *data[4]) override;

private:
    IX_DISABLE_COPY(iAbstractPlanarVideoBuffer)
};

} // namespace iShell

#endif // IABSTRACTVIDEOBUFFER_H
