/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imemoryvideobuffer_p.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IMEMORYVIDEOBUFFER_P_H
#define IMEMORYVIDEOBUFFER_P_H

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

#include <core/utils/ibitarray.h>
#include <multimedia/video/iabstractvideobuffer.h>

namespace iShell {

class iMemoryVideoBuffer : public iAbstractVideoBuffer
{
public:
    iMemoryVideoBuffer(const iByteArray &data, int bytesPerLine);
    ~iMemoryVideoBuffer();

    MapMode mapMode() const override;

    uchar *map(MapMode mode, int *numBytes, int *bytesPerLine) override;
    void unmap() override;

protected:
    int m_bytesPerLine;
    MapMode m_mapMode;
    iByteArray m_data;
};

} // namespace iShell

#endif // IMEMORYVIDEOBUFFER_P_H
