/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstreamermessage.h
/// @brief   provides a wrapper around GstMessage type
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include <gst/gst.h>

#include <core/global/imacro.h>
#include "igstreamermessage_p.h"

namespace iShell {

iGstreamerMessage::iGstreamerMessage():
    m_message(IX_NULLPTR)
{
}

iGstreamerMessage::iGstreamerMessage(GstMessage* message):
    m_message(message)
{
    gst_message_ref(m_message);
}

iGstreamerMessage::iGstreamerMessage(iGstreamerMessage const& m):
    m_message(m.m_message)
{
    gst_message_ref(m_message);
}


iGstreamerMessage::~iGstreamerMessage()
{
    if (m_message != IX_NULLPTR)
        gst_message_unref(m_message);
}

GstMessage* iGstreamerMessage::rawMessage() const
{
    return m_message;
}

iGstreamerMessage& iGstreamerMessage::operator=(iGstreamerMessage const& rhs)
{
    if (rhs.m_message != m_message) {
        if (rhs.m_message != IX_NULLPTR)
            gst_message_ref(rhs.m_message);

        if (m_message != IX_NULLPTR)
            gst_message_unref(m_message);

        m_message = rhs.m_message;
    }

    return *this;
}

} // namespace iShell
