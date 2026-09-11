#include <gtest/gtest.h>
#include <multimedia/imediaobject.h>
#include <core/kernel/ieventdispatcher.h>
#include <chrono>

using namespace iShell;

class WatchedMedia : public iMediaObject {
    IX_OBJECT(WatchedMedia)
    IPROPERTY_BEGIN
    IPROPERTY_ITEM("value", IREAD value, INOTIFY valueChanged)
    IPROPERTY_END
public:
    WatchedMedia() : iMediaObject(nullptr) {}
    int value() const { return 7; }
    void valueChanged(int value) ISIGNAL(valueChanged, value)
    void watch() { setNotifyInterval(1); addPropertyWatch(iByteArray("value")); }
    void unwatch() { removePropertyWatch(iByteArray("value")); }
};

class MediaNotificationReceiver : public iObject {
public:
    WatchedMedia* media = nullptr;
    int calls = 0;
    bool destroy = false;
    void receive(int value) {
        EXPECT_EQ(7, value);
        ++calls;
        if (destroy) {
            delete media;
            media = nullptr;
        } else {
            media->unwatch();
        }
    }
};

TEST(MediaNotificationRegression, HandlerCanRemoveCurrentPropertyWatch)
{
    WatchedMedia media;
    MediaNotificationReceiver receiver;
    receiver.media = &media;
    ASSERT_TRUE(iObject::connect(&media, &WatchedMedia::valueChanged, &receiver, &MediaNotificationReceiver::receive));
    media.watch();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (receiver.calls == 0 && std::chrono::steady_clock::now() < deadline)
        iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
    EXPECT_EQ(1, receiver.calls);
    for (int index = 0; index < 10; ++index)
        iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
    EXPECT_EQ(1, receiver.calls);
}

TEST(MediaNotificationRegression, HandlerCanDeleteMediaObject)
{
    MediaNotificationReceiver receiver;
    receiver.media = new WatchedMedia;
    receiver.destroy = true;
    ASSERT_TRUE(iObject::connect(receiver.media, &WatchedMedia::valueChanged, &receiver, &MediaNotificationReceiver::receive));
    receiver.media->watch();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(1);
    while (receiver.calls == 0 && std::chrono::steady_clock::now() < deadline)
        iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
    EXPECT_EQ(1, receiver.calls);
    EXPECT_EQ(nullptr, receiver.media);
    delete receiver.media;
}