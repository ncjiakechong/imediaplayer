/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imediapluginfactory.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IMEDIAPLUGINFACTORY_H
#define IMEDIAPLUGINFACTORY_H

#include <multimedia/controls/imediaplayercontrol.h>

namespace iShell {

class iMediaPluginFactory
{
public:
    static iMediaPluginFactory* instance();

    ~iMediaPluginFactory();

    iMediaPlayerControl* createControl(iObject* parent = IX_NULLPTR);
    iObject* createVideoOutput(iObject* parent = IX_NULLPTR);

private:
    iMediaPluginFactory();
    IX_DISABLE_COPY(iMediaPluginFactory)
};

} // namespace iShell

#endif
