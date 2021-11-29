/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imediapluginfactory.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IMEDIAPLUGINFACTORY_H
#define IMEDIAPLUGINFACTORY_H

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
