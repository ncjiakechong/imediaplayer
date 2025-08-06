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
        iObject::connect(player, &iMediaPlayer::stateChanged, this, &TestPlayer::stateChanged);
        iObject::connect(player, &iMediaPlayer::errorEvent, this, &TestPlayer::errorEvent);
        iObject::connect(player, &iMediaPlayer::positionChanged, this, &TestPlayer::positionChanged);
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

    int play() {
//        player->setMedia(iUrl("gst-pipeline:playbin uri=\"file:///home/jiakechong/Downloads/Video.mp4\""));
        player->setMedia(iUrl("file:///home/jiakechong/Downloads/Video.mp4"));
//        player->setMedia(iUrl("file:///home/jiakechong/Downloads/thatgirl.mp3"));
        player->play();

        if (iMediaPlayer::StoppedState != player->state())
            return 0;

        return -1;
    }

    iMediaPlayer* player;
};

int test_player(void)
{
    TestPlayer* player = new TestPlayer();
    return player->play();
}
