/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iabstractvideobuffer.cpp
/// @brief   an abstraction for video data
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "core/kernel/ivariant.h"
#include "multimedia/video/iabstractvideobuffer.h"

#include "iabstractvideobuffer_p.h"

namespace iShell {

/*!
    \class iAbstractVideoBuffer
    \brief The iAbstractVideoBuffer class is an abstraction for video data.
    \ingroup multimedia
    \ingroup multimedia_video

    The iVideoFrame class makes use of a iAbstractVideoBuffer internally to reference a buffer of
    video data.  Quite often video data buffers may reside in video memory rather than system
    memory, and this class provides an abstraction of the location.

    In addition, creating a subclass of iAbstractVideoBuffer will allow you to construct video
    frames from preallocated or static buffers, in cases where the iVideoFrame constructors
    taking a iByteArray or a iImage do not suffice.  This may be necessary when implementing
    a new hardware accelerated video system, for example.

    The contents of a buffer can be accessed by mapping the buffer to memory using the map()
    function, which returns a pointer to memory containing the contents of the video buffer.
    The memory returned by map() is released by calling the unmap() function.

    The handle() of a buffer may also be used to manipulate its contents using type specific APIs.
    The type of a buffer's handle is given by the handleType() function.

    \sa iVideoFrame
*/

/*!
    \enum iAbstractVideoBuffer::HandleType

    Identifies the type of a video buffers handle.

    \value NoHandle The buffer has no handle, its data can only be accessed by mapping the buffer.
    \value GLTextureHandle The handle of the buffer is an OpenGL texture ID.
    \value XvShmImageHandle The handle contains pointer to shared memory XVideo image.
    \value CoreImageHandle The handle contains pointer to \macos CIImage.
    \value iPixmapHandle The handle of the buffer is a iPixmap.
    \value EGLImageHandle The handle of the buffer is an EGLImageKHR.
    \value UserHandle Start value for user defined handle types.

    \sa handleType()
*/

/*!
    \enum iAbstractVideoBuffer::MapMode

    Enumerates how a video buffer's data is mapped to system memory.

    \value NotMapped The video buffer is not mapped to memory.
    \value ReadOnly The mapped memory is populated with data from the video buffer when mapped, but
    the content of the mapped memory may be discarded when unmapped.
    \value WriteOnly The mapped memory is uninitialized when mapped, but the possibly modified content
    will be used to populate the video buffer when unmapped.
    \value ReadWrite The mapped memory is populated with data from the video buffer, and the
    video buffer is repopulated with the content of the mapped memory when it is unmapped.

    \sa mapMode(), map()
*/

/*!
    Constructs an abstract video buffer of the given \a type.
*/
iAbstractVideoBuffer::iAbstractVideoBuffer(HandleType type)
    : m_type(type)
{
}

/*!
    Destroys an abstract video buffer.
*/
iAbstractVideoBuffer::~iAbstractVideoBuffer()
{
}

/*!
    Releases the video buffer.

    iVideoFrame calls iAbstractVideoBuffer::release when the buffer is not used
    any more and can be destroyed or returned to the buffer pool.

    The default implementation deletes the buffer instance.
*/
void iAbstractVideoBuffer::release()
{
    delete this;
}

/*!
    Returns the type of a video buffer's handle.

    \sa handle()
*/
iAbstractVideoBuffer::HandleType iAbstractVideoBuffer::handleType() const
{
    return m_type;
}

/*!
    \fn iAbstractVideoBuffer::mapMode() const

    Returns the mode a video buffer is mapped in.

    \sa map()
*/

/*!
    \fn iAbstractVideoBuffer::map(MapMode mode, int *numBytes, int *bytesPerLine)

    Maps the contents of a video buffer to memory.

    In some cases the video buffer might be stored in video memory or otherwise inaccessible
    memory, so it is necessary to map the buffer before accessing the pixel data.  This may involve
    copying the contents around, so avoid mapping and unmapping unless required.

    The map \a mode indicates whether the contents of the mapped memory should be read from and/or
    written to the buffer.  If the map mode includes the \c iAbstractVideoBuffer::ReadOnly flag the
    mapped memory will be populated with the content of the buffer when initially mapped.  If the map
    mode includes the \c iAbstractVideoBuffer::WriteOnly flag the content of the possibly modified
    mapped memory will be written back to the buffer when unmapped.

    When access to the data is no longer needed be sure to call the unmap() function to release the
    mapped memory and possibly update the buffer contents.

    Returns a pointer to the mapped memory region, or a null pointer if the mapping failed.  The
    size in bytes of the mapped memory region is returned in \a numBytes, and the line stride in \a
    bytesPerLine.

    \note Writing to memory that is mapped as read-only is undefined, and may result in changes
    to shared data or crashes.

    \sa unmap(), mapMode()
*/


/*!
    Independently maps the planes of a video buffer to memory.

    The map \a mode indicates whether the contents of the mapped memory should be read from and/or
    written to the buffer.  If the map mode includes the \c iAbstractVideoBuffer::ReadOnly flag the
    mapped memory will be populated with the content of the buffer when initially mapped.  If the map
    mode includes the \c iAbstractVideoBuffer::WriteOnly flag the content of the possibly modified
    mapped memory will be written back to the buffer when unmapped.

    When access to the data is no longer needed be sure to call the unmap() function to release the
    mapped memory and possibly update the buffer contents.

    Returns the number of planes in the mapped video data.  For each plane the line stride of that
    plane will be returned in \a bytesPerLine, and a pointer to the plane data will be returned in
    \a data.  The accumulative size of the mapped data is returned in \a numBytes.

    Not all buffer implementations will map more than the first plane, if this returns a single
    plane for a planar format the additional planes will have to be calculated from the line stride
    of the first plane and the frame height.  Mapping a buffer with iVideoFrame will do this for
    you.

    To implement this function create a derivative of iAbstractPlanarVideoBuffer and implement
    its map function instance instead.
*/
int iAbstractVideoBuffer::mapPlanes(MapMode mode, int *numBytes, int bytesPerLine[4], uchar *data[4])
{
    return mapImpl(mode, numBytes, bytesPerLine, data);
}

int iAbstractVideoBuffer::mapImpl(MapMode mode, int *numBytes, int bytesPerLine[4], uchar *data[4])
{
    data[0] = map(mode, numBytes, bytesPerLine);
    return data[0] ? 1 : 0;
}

/*!
    \fn iAbstractVideoBuffer::unmap()

    Releases the memory mapped by the map() function.

    If the \l {iAbstractVideoBuffer::MapMode}{MapMode} included the \c iAbstractVideoBuffer::WriteOnly
    flag this will write the current content of the mapped memory back to the video frame.

    \sa map()
*/

/*!
    Returns a type specific handle to the data buffer.

    The type of the handle is given by handleType() function.

    \sa handleType()
*/
iVariant iAbstractVideoBuffer::handle() const
{
    return iVariant();
}

/*!
    \class iAbstractPlanarVideoBuffer
    \brief The iAbstractPlanarVideoBuffer class is an abstraction for planar video data.
    \ingroup multimedia
    \ingroup multimedia_video

    iAbstractPlanarVideoBuffer extends iAbstractVideoBuffer to support mapping
    non-continuous planar video data.  Implement this instead of iAbstractVideoBuffer when the
    abstracted video data stores planes in separate buffers or includes padding between planes
    which would interfere with calculating offsets from the bytes per line and frame height.

    \sa iAbstractVideoBuffer::mapPlanes()
*/

/*!
    Constructs an abstract planar video buffer of the given \a type.
*/
iAbstractPlanarVideoBuffer::iAbstractPlanarVideoBuffer(HandleType type)
    : iAbstractVideoBuffer(type)
{
}

/*!
    Destroys an abstract planar video buffer.
*/
iAbstractPlanarVideoBuffer::~iAbstractPlanarVideoBuffer()
{
}

int iAbstractPlanarVideoBuffer::mapImpl(MapMode mode, int *numBytes, int bytesPerLine[4], uchar *data[4])
{
    return map(mode, numBytes, bytesPerLine, data);
}

/*!
    \internal
*/
uchar *iAbstractPlanarVideoBuffer::map(MapMode mode, int *numBytes, int *bytesPerLine)
{
    uchar *data[4];
    int strides[4];
    if (map(mode, numBytes, strides, data) > 0) {
        if (bytesPerLine)
            *bytesPerLine = strides[0];
        return data[0];
    } else {
        return IX_NULLPTR;
    }
}

/*!
    \fn int iAbstractPlanarVideoBuffer::map(MapMode mode, int *numBytes, int bytesPerLine[4], uchar *data[4])

    Maps the contents of a video buffer to memory.

    The map \a mode indicates whether the contents of the mapped memory should be read from and/or
    written to the buffer.  If the map mode includes the \c iAbstractVideoBuffer::ReadOnly flag the
    mapped memory will be populated with the content of the buffer when initially mapped.  If the map
    mode includes the \c iAbstractVideoBuffer::WriteOnly flag the content of the possibly modified
    mapped memory will be written back to the buffer when unmapped.

    When access to the data is no longer needed be sure to call the unmap() function to release the
    mapped memory and possibly update the buffer contents.

    Returns the number of planes in the mapped video data.  For each plane the line stride of that
    plane will be returned in \a bytesPerLine, and a pointer to the plane data will be returned in
    \a data.  The accumulative size of the mapped data is returned in \a numBytes.

    \sa iAbstractVideoBuffer::map(), iAbstractVideoBuffer::unmap(), iAbstractVideoBuffer::mapMode()
*/

} // namespace iShell
