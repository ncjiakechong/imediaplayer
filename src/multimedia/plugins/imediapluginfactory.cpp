/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imediapluginfactory.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////

#include "plugins/gstreamer/igstutils_p.h"
#include "plugins/gstreamer/igstreamerplayersession_p.h"
#include "plugins/gstreamer/igstreamerplayercontrol_p.h"
#include "plugins/gstreamer/igstreamerautorenderer.h"
#include "plugins/imediapluginfactory.h"

namespace iShell {

iMediaPluginFactory::iMediaPluginFactory()
{
    iGstUtils::initializeGst();
}

iMediaPluginFactory::~iMediaPluginFactory()
{
}

iMediaPluginFactory* iMediaPluginFactory::instance()
{
    static iMediaPluginFactory* s_instance = IX_NULLPTR;

    static struct Cleanup {
        ~Cleanup() {
            delete s_instance;
        }
    } cleanup; 

    if (IX_NULLPTR == s_instance)
        s_instance = new iMediaPluginFactory();

    return s_instance;
}

iMediaPlayerControl* iMediaPluginFactory::createControl(iObject* parent)
{
    iGstreamerPlayerSession* session = new iGstreamerPlayerSession(parent);
    iGstreamerPlayerControl* control = new iGstreamerPlayerControl(session, parent);
    return control;
}

iObject* iMediaPluginFactory::createVideoOutput(iObject* parent)
{
    return new iGstreamerAutoRenderer(parent);
}

} // namespace iShell
