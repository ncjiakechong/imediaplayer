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
    // to verify zero copy in iIODevice
    std::unordered_map<const iMemBlock*, xint64> m_createdBuffer;

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
        if (mode != ReadOnly && mode != ReadWrite)
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


        setWriteChannelCount(0);
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

    virtual iByteArray readData(xint64 maxlen, xint64* readLen)
    {
        xint64 dummy = 0;
        iByteArray buffer;
        if (!readLen) readLen = &dummy;

        buffer.reserve(std::max<xint64>(maxlen, 256));
        *readLen = ::read(m_fd, buffer.data(), buffer.capacity());
        m_createdBuffer.insert({buffer.data_ptr().d_ptr(), *readLen});

        if (*readLen > 0) {
            m_currPos += *readLen;
            buffer.resize(*readLen);
        }

        if (maxlen > 0)
            return buffer;

        ilog_info("try to ", *readLen, " buffer the stream data!!!");
        if (*readLen > 0 && 0 >= maxlen) {
            m_buffer.append(buffer);
        }

        return iByteArray();
    }
    virtual xint64 writeData(const iByteArray& data)
    {
        IX_ASSERT(m_createdBuffer.size() > 0);
        std::unordered_map<const iMemBlock*, xint64>::iterator it = m_createdBuffer.find(data.data_ptr().d_ptr());
        IX_ASSERT(it != m_createdBuffer.end());
        it->second -= data.length();
        if (it->second <= 0)
            m_createdBuffer.erase(it);

        return data.length();
    }

    void noMoreData() const ISIGNAL(noMoreData);
};

class TestPlayer : public iObject
{
    IX_OBJECT(TestPlayer)
public:
    TestPlayer(iObject* parent = IX_NULLPTR)
    : iObject(parent)
    , m_loopTime(0)
    , m_ioTimer(0)
    , m_fileSize(0)
    , varifyIO(IX_NULLPTR)
    , streamDevice(IX_NULLPTR)
    {
        player = new iMediaPlayer(this);
        player->observeProperty("state", this, &TestPlayer::stateChanged);
        player->observeProperty("position", this, &TestPlayer::positionChanged);
        iObject::connect(player, &iMediaPlayer::errorEvent, this, &TestPlayer::errorEvent);
    }

    ~TestPlayer()
    {
    }

    virtual bool event(iEvent *e) {
        if (e->type() != iEvent::Timer) {
            return iObject::event(e);
        }

        iTimerEvent* event = static_cast<iTimerEvent*>(e);
        if (m_ioTimer != event->timerId())
            return true;

        iByteArray data = varifyIO->read(1024);
        m_fileSize += data.length();
        if (data.isEmpty()) {
            ilog_info("player varifyIO filesize: ", m_fileSize);
            killTimer(m_ioTimer);
            varifyIO->close();
            m_ioTimer = 0;
            return true;
        }

        varifyIO->write(data);
        return true;
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
        varifyIO = new TestStreamDevice(path, this);
        varifyIO->open(iIODevice::ReadWrite);
        m_ioTimer = startTimer(10);
        m_fileSize = 0;

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
    int m_ioTimer;
    int m_fileSize;

    TestStreamDevice* varifyIO;
    TestStreamDevice* streamDevice;
    iMediaPlayer* player;
};

int test_player(const iString& path, void (*callback)())
{
    TestPlayer* player = new TestPlayer();
    iObject::connect(player, &TestPlayer::destroyed, callback);
    return player->play(path);
}
