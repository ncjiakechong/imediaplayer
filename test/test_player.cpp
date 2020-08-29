/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    test_player.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////

#include <core/kernel/icoreapplication.h>
#include <multimedia/playback/imediaplayer.h>
#include "core/io/ilog.h"

#define ILOG_TAG "test"

using namespace iShell;

class TestPlayer : public iObject
{
    IX_OBJECT(TestPlayer)
public:
    TestPlayer(iObject* parent = IX_NULLPTR)
    : iObject(parent)
    {
        player = new iMediaPlayer();
        player->observeProperty("state", this, &TestPlayer::stateChanged);
        player->observeProperty("position", this, &TestPlayer::positionChanged);
        iObject::connect(player, &iMediaPlayer::errorEvent, this, &TestPlayer::errorEvent);
    }

    void errorEvent(iMediaPlayer::Error errorNum)
    {
        ilog_warn(__FUNCTION__, ": ", errorNum);
    }

    void stateChanged(iMediaPlayer::State newState)
    {
        ilog_debug(__FUNCTION__, ": ", newState);
        if (newState == iMediaPlayer::StoppedState) {
            deleteLater();
            iCoreApplication::quit();
        }
    }

    void positionChanged(xint64 position)
    {
        ilog_debug(__FUNCTION__, ": ", position, "/", player->duration());
    }

    int play(const iString& path) {
        player->setMedia(iUrl(path));
        player->play();

        if (iMediaPlayer::StoppedState != player->state())
            return 0;

        return -1;
    }

    iMediaPlayer* player;
};

int test_player(const iString& path)
{
    TestPlayer* player = new TestPlayer();
    return player->play(path);
}
