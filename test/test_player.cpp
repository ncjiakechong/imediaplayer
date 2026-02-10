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
#include <map>
#include <vector>
#include <list>

#define ILOG_TAG "test"

using namespace iShell;

struct PlayerConfig {
    iString url;
    int loopCount;           // Default loop 1 time (play once)
    bool enableVerify;   // Enable raw IO verification
    bool useCustomIO;    // Use custom IO device (appsrc://) or default file playback
    int ioInterval;         // IO check interval in ms

    PlayerConfig() : loopCount(1), enableVerify(false), useCustomIO(false), ioInterval(10) {}
};

class TestStreamDevice : public iIODevice
{
    IX_OBJECT(TestStreamDevice)
public:
    bool m_trackIOMem;
    int m_fd;
    xint64 m_currPos;
    iString m_filePath;
    // to verify zero copy in iIODevice
    std::map<const iMemBlock*, xint64> m_verifiedBuffer;

    TestStreamDevice(const iString& path, iObject *parent = IX_NULLPTR)
        : iIODevice(parent), m_trackIOMem(false), m_fd(-1), m_currPos(0), m_filePath(path)
    {}

    virtual ~TestStreamDevice()
    {
        close();
    }

    void setTrackIOMem(bool track)
    {
        m_trackIOMem = track;
    }

    bool isSequential() const IX_OVERRIDE
    {
        return true;
    }

    bool open(OpenMode mode) IX_OVERRIDE
    {
        if (mode != ReadOnly && mode != ReadWrite)
            return false;

        iIODevice::open(mode);
        iUrl filename(m_filePath);
        m_currPos = 0;
        m_fd = ::open(filename.toLocalFile().toUtf8().data(), O_RDONLY);

        return (-1 != m_fd);
    }

    void close() IX_OVERRIDE
    {
        if (-1 == m_fd)
            return;

        ::close(m_fd);
        m_fd = -1;

        clearWriteChannels();
        iIODevice::close();
    }

    xint64 size() const IX_OVERRIDE {
        struct stat stat_results;
        if (fstat (m_fd, &stat_results) < 0)
            return 0;

        return stat_results.st_size;
    }

    xint64 bytesAvailable() const IX_OVERRIDE
    {
        struct stat stat_results;
        if (fstat (m_fd, &stat_results) < 0)
            return 0;

        xint64 remainSize = stat_results.st_size - m_currPos;
        if (remainSize <= 0)
            IEMIT noMoreData();

        return remainSize;
    }

    iByteArray readData(xint64 maxlen, xint64* readLen) IX_OVERRIDE
    {
        xint64 dummy = 0;
        iByteArray buffer;
        if (!readLen) readLen = &dummy;

        buffer.reserve(std::max<xint64>(maxlen, 256));
        *readLen = ::read(m_fd, buffer.data(), buffer.capacity());

        if (m_trackIOMem)
            m_verifiedBuffer.insert(std::make_pair(buffer.data_ptr().d_ptr(), *readLen));

        if (*readLen > 0) {
            m_currPos += *readLen;
            buffer.resize(*readLen);
        }

        if (maxlen > 0)
            return buffer;

        if (*readLen > 0 && 0 >= maxlen) {
            m_buffer.append(buffer);
        }

        return iByteArray();
    }
    xint64 writeData(const iByteArray& data) IX_OVERRIDE
    {
        IX_ASSERT(m_verifiedBuffer.size() > 0);
        std::map<const iMemBlock*, xint64>::iterator it = m_verifiedBuffer.find(data.data_ptr().d_ptr());
        IX_ASSERT(it != m_verifiedBuffer.end());
        it->second -= data.length();
        if (it->second <= 0)
            m_verifiedBuffer.erase(it);

        return data.length();
    }

    void noMoreData() const ISIGNAL(noMoreData);
};

class TestPlayer : public iObject
{
    IX_OBJECT(TestPlayer)
public:
    TestPlayer(const PlayerConfig& config, iObject* parent = IX_NULLPTR)
    : iObject(parent)
    , m_config(config)
    , m_loopCount(0)
    , m_ioTimer(0)
    , m_fileSize(0)
    , verifyIO(IX_NULLPTR)
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

    bool event(iEvent *e) IX_OVERRIDE {
        if (e->type() != iEvent::Timer) {
            return iObject::event(e);
        }

        iTimerEvent* event = static_cast<iTimerEvent*>(e);
        if (m_ioTimer != event->timerId())
            return true;

        iByteArray data = verifyIO->read(2048);
        m_fileSize += data.length();
        if (data.isEmpty()) {
            ilog_debug("player verifyIO filesize: ", m_fileSize);
            killTimer(m_ioTimer);
            verifyIO->close();
            m_ioTimer = 0;
            return true;
        }

        verifyIO->write(data);
        return true;
    }
    void errorEvent(iMediaPlayer::Error errorNum)
    {
        ilog_warn("MediaPlayer Error: ", errorNum);
    }

    void stateChanged(iMediaPlayer::State newState)
    {
        ilog_debug("State Changed: ", newState);
        if (newState != iMediaPlayer::StoppedState)
            return;

        m_loopCount++;
        ilog_info("Playback finished. Iteration: ", m_loopCount, "/", m_config.loopCount);

        if (m_loopCount < m_config.loopCount) {
             iObject::invokeMethod(this, &TestPlayer::rePlay, QueuedConnection);
             return;
        }

        player->stop();
        iTimer::singleShot(500, 0, this, &TestPlayer::deleteLater);
    }

    void positionChanged(xint64 position)
    {
        if (player->duration() > 0) {
            ilog_debug(position / 1000.0, "s /", player->duration() / 1000.0, "s");
        }
    }

    int play() {
        if (m_config.enableVerify) {
            verifyIO = new TestStreamDevice(m_config.url, this);
            verifyIO->setTrackIOMem(true);
            if (verifyIO->open(iIODevice::ReadWrite)) {
                m_ioTimer = startTimer(m_config.ioInterval);
                m_fileSize = 0;
            } else {
                 ilog_error("Failed to open file for IO verification: ", m_config.url);
                 return -1;
            }
        }

        if (m_config.useCustomIO) {
            streamDevice = new TestStreamDevice(m_config.url, this);
            if (!streamDevice->open(iIODevice::ReadOnly)) {
                 ilog_error("Failed to open stream device: ", m_config.url);
                 return -1;
            }
            iObject::connect(streamDevice, &TestStreamDevice::noMoreData, player, &iMediaPlayer::stop);
            player->setMedia(iUrl("appsrc://"), streamDevice);
        } else {
            player->setMedia(iUrl(m_config.url));
        }

        player->play();

        if (iMediaPlayer::StoppedState != player->state())
            return 0;

        return -1;
    }

    void rePlay() {
        if (streamDevice) {
             streamDevice->close();
             streamDevice->open(iIODevice::ReadOnly);
        }
        player->play();
    }

    PlayerConfig m_config;
    int m_loopCount;
    int m_ioTimer;
    xint64 m_fileSize;

    TestStreamDevice* verifyIO;
    TestStreamDevice* streamDevice;
    iMediaPlayer* player;
};

int test_player(void (*callback)())
{
    std::list<iShell::iString> argsList = iShell::iCoreApplication::arguments();
    std::vector<iShell::iString> args(argsList.begin(), argsList.end());

    bool enablePlay = false;
    PlayerConfig config;
    config.loopCount = 1; // Default 1 run

    for (size_t i = 1; i < args.size(); ++i) {
        if (args[i] == "--play") {
            enablePlay = true;
            if (i + 1 < args.size() && !args[i+1].startsWith("-")) {
                config.url = args[++i];
            }
        }
        else if (args[i] == "--loop" && i + 1 < args.size()) {
            config.loopCount = args[++i].toInt();
        }
        else if (args[i] == "--verify") {
            config.enableVerify = true;
        }
        else if (args[i] == "--custom-io") {
            config.useCustomIO = true;
        }
        else if (args[i] == "--interval" && i + 1 < args.size()) {
            config.ioInterval = args[++i].toInt();
        }
    }

    if (!enablePlay) {
        // Not running this test
        return -1;
    }

    if (config.url.isEmpty()) {
        ilog_info("Usage: imediaplayertest --play <url> [--loop <count>] [--verify] [--custom-io] [--interval <ms>]");
        ilog_info("Example: imediaplayertest --play /tmp/test.mp4 --loop 5 --verify");
        return -1;
    }

    TestPlayer* player = new TestPlayer(config);
    iObject::connect(player, &TestPlayer::destroyed, callback);
    return player->play();
}
