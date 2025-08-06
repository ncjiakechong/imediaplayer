/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iabstractvideobuffer.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IABSTRACTVIDEOBUFFER_H
#define IABSTRACTVIDEOBUFFER_H

#include <core/global/iglobal.h>
#include <core/global/imacro.h>

namespace iShell {


class iVariant;

class iAbstractVideoBufferPrivate;

class iAbstractVideoBuffer
{
public:
    enum HandleType
    {
        NoHandle,
        GLTextureHandle,
        XvShmImageHandle,
        CoreImageHandle,
        QPixmapHandle,
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
    iAbstractVideoBuffer(iAbstractVideoBufferPrivate &dd, HandleType type);

    iAbstractVideoBufferPrivate *d_ptr;  // For expansion, not used currently
    HandleType m_type;

private:
    friend class iAbstractVideoBufferPrivate;
    IX_DISABLE_COPY(iAbstractVideoBuffer)
};

class iAbstractPlanarVideoBufferPrivate;
class iAbstractPlanarVideoBuffer : public iAbstractVideoBuffer
{
public:
    iAbstractPlanarVideoBuffer(HandleType type);
    virtual ~iAbstractPlanarVideoBuffer();

    uchar *map(MapMode mode, int *numBytes, int *bytesPerLine) override;
    virtual int map(MapMode mode, int *numBytes, int bytesPerLine[4], uchar *data[4]) = 0;

protected:
    iAbstractPlanarVideoBuffer(iAbstractPlanarVideoBufferPrivate &dd, HandleType type);

private:
    IX_DISABLE_COPY(iAbstractPlanarVideoBuffer)
};

} // namespace iShell

#endif // IABSTRACTVIDEOBUFFER_H
