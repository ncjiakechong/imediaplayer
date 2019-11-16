/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstcodecsinfo_p.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IGSTCODECSINFO_H
#define IGSTCODECSINFO_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the API.  It exists purely as an
// implementation detail.  This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <map>

#include <gst/gst.h>

#include <core/utils/istring.h>

namespace iShell {

class iGstCodecsInfo
{
public:
    enum ElementType { AudioEncoder, VideoEncoder, Muxer };

    struct CodecInfo {
        iString description;
        iByteArray elementName;
        GstRank rank;
    };

    iGstCodecsInfo(ElementType elementType);

    std::list<iString> supportedCodecs() const;
    iString codecDescription(const iString &codec) const;
    iByteArray codecElement(const iString &codec) const;
    std::list<iString> codecOptions(const iString &codec) const;

private:
    void updateCodecs(ElementType elementType);
    GList *elementFactories(ElementType elementType) const;

    std::list<iString> m_codecs;
    std::multimap<iString, CodecInfo> m_codecInfo;
};

} // namespace iShell

#endif
