/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    test_player.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#include <core/kernel/icoreapplication.h>
#include <multimedia/playback/imediaplayer.h>
#include <core/io/iiodevice.h>
#include <core/kernel/ievent.h>
#include "core/io/ilog.h"

#define ILOG_TAG "test"

using namespace iShell;


class TestStreamDevice : public iIODevice
{
    IX_OBJECT(TestStreamDevice)
public:
    iString m_filePath;
    int m_fd;
    xint64 m_currPos;

    TestStreamDevice(const iString& path, iObject *parent = IX_NULLPTR) : iIODevice(parent), m_filePath(path), m_fd(0), m_currPos(0) {}
    virtual ~TestStreamDevice() 
    {
        close();
    }

    virtual bool isSequential() const
    {
        return true;
    }

    virtual bool open(OpenMode mode)
    {
        if (mode != ReadOnly)
            return false;
        
        iIODevice::open(mode);
        iUrl filename(m_filePath);
        m_currPos = 0;
        m_fd = ::open(filename.toLocalFile().toUtf8().data(), O_RDONLY);

        return (-1 != m_fd);
    }

    virtual void close()
    {
        if (-1 == m_fd)
            return;
        
        ::close(m_fd);
        m_fd = -1;

        iIODevice::close();
    }

    virtual xint64 size() const {
        struct stat stat_results;
        if (fstat (m_fd, &stat_results) < 0)
            return 0;

        return stat_results.st_size;
    }

    virtual xint64 bytesAvailable() const
    {
        struct stat stat_results;
        if (fstat (m_fd, &stat_results) < 0)
            return 0;

        xint64 remainSize = stat_results.st_size - m_currPos;
        if (remainSize <= 0)
            IEMIT noMoreData();

        return remainSize;
    }

    virtual xint64 readData(char *data, xint64 maxlen)
    {
        xint64 len = ::read(m_fd, data, maxlen);
        if (len > 0)
            m_currPos += len;

        if (maxlen > 0)
            return len;

        ilog_info("try to buffer the stream data!!!");
        char buffer[256];
        xint64 len2 = ::read(m_fd, buffer, sizeof(buffer));
        if (len2 > 0) {
            m_buffer.append(buffer, len2);
            m_currPos += len2;
        }

        return 0;
    }
    virtual xint64 writeData(const char *data, xint64 len)
    { return 0; }

    void noMoreData() const ISIGNAL(noMoreData);
};

class TestPlayer : public iObject
{
    IX_OBJECT(TestPlayer)
public:
    TestPlayer(iObject* parent = IX_NULLPTR)
    : iObject(parent)
    , m_loopTime(0)
    {
        player = new iMediaPlayer(this);
        player->observeProperty("state", this, &TestPlayer::stateChanged);
        player->observeProperty("position", this, &TestPlayer::positionChanged);
        iObject::connect(player, &iMediaPlayer::errorEvent, this, &TestPlayer::errorEvent);
    }

    ~TestPlayer()
    {
    }

    void errorEvent(iMediaPlayer::Error errorNum)
    {
        ilog_warn(": ", errorNum);
    }

    void stateChanged(iMediaPlayer::State newState)
    {
        ilog_debug(newState);
        if (newState != iMediaPlayer::StoppedState)
            return;

        if (2 >= m_loopTime) {
            ++ m_loopTime;
            ilog_info("player replay times ----", m_loopTime, "----");
            iObject::invokeMethod(this, &TestPlayer::rePlay, QueuedConnection);
            return;
        }

        player->stop();
        iTimer::singleShot(500, 0, this, &TestPlayer::deleteLater);
    }

    void positionChanged(xint64 position)
    {
        ilog_debug(position / 1000.0, "/", player->duration() / 1000.0);
    }

    int play(const iString& path) {
        streamDevice = new TestStreamDevice(path, this);
        streamDevice->open(iIODevice::ReadOnly);
        iObject::connect(streamDevice, &TestStreamDevice::noMoreData, player, &iMediaPlayer::stop);

        player->setMedia(iUrl(path));
        // player->setMedia(iUrl("appsrc://"), streamDevice);
        player->play();

        if (iMediaPlayer::StoppedState != player->state())
            return 0;

        return -1;
    }

    void rePlay() {
        iString path = streamDevice->m_filePath;
        if (m_loopTime & 1) {
            streamDevice->close();
            streamDevice->open(iIODevice::ReadOnly);
            player->setMedia(iUrl("appsrc://"), streamDevice);
        } else {
            player->setMedia(iUrl(path));
        }
        player->play();
    }

    int m_loopTime;

    TestStreamDevice* streamDevice;
    iMediaPlayer* player;
};

int test_player(const iString& path, void (*callback)())
{
    TestPlayer* player = new TestPlayer();
    iObject::connect(player, &TestPlayer::destroyed, callback);
    return player->play(path);
}
