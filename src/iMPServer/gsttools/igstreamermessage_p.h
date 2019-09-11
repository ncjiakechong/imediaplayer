/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstreamermessage_p.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IGSTREAMERMESSAGE_P_H
#define IGSTREAMERMESSAGE_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the API. It exists purely as an
// implementation detail. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <gst/gst.h>

namespace iShell {

class iGstreamerMessage
{
public:
    iGstreamerMessage();
    iGstreamerMessage(GstMessage* message);
    iGstreamerMessage(iGstreamerMessage const& m);
    ~iGstreamerMessage();

    GstMessage* rawMessage() const;

    iGstreamerMessage& operator=(iGstreamerMessage const& rhs);

private:
    GstMessage* m_message;
};

} // namespace iShell


#endif
