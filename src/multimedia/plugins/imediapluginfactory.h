/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    imediapluginfactory.h
/// @brief   factory for creating multimedia-related objects
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
#include <multimedia/controls/imediarecordercontrol.h>
#include <multimedia/controls/ivideodecodercontrol.h>
#include <multimedia/controls/ivideoencodercontrol.h>
#include <multimedia/controls/ivideosinkcontrol.h>

namespace iShell {

class iMediaPluginFactory
{
public:
    static iMediaPluginFactory* instance();

    ~iMediaPluginFactory();

    iMediaPlayerControl* createControl(iObject* parent = IX_NULLPTR);
    iObject* createVideoOutput(iObject* parent = IX_NULLPTR);

    iVideoDecoderControl* createVideoDecoderControl(iObject* parent = IX_NULLPTR);
    iVideoEncoderControl* createVideoEncoderControl(iObject* parent = IX_NULLPTR);
    iVideoSinkControl* createVideoSinkControl(iObject* parent = IX_NULLPTR);
    iMediaRecorderControl* createMediaRecorderControl(iObject* parent = IX_NULLPTR);

private:
    iMediaPluginFactory();
    IX_DISABLE_COPY(iMediaPluginFactory)
};

} // namespace iShell

#endif
