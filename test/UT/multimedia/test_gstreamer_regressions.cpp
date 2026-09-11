#include <gtest/gtest.h>
#include <core/kernel/icoreapplication.h>
#include <core/kernel/ieventdispatcher.h>
#include <multimedia/plugins/gstreamer/igstreamerbushelper_p.h>
#include <multimedia/plugins/gstreamer/igstreamerplayersession_p.h>
#include <multimedia/plugins/gstreamer/igstreamerplayercontrol_p.h>
#include <multimedia/plugins/gstreamer/igstreamervideorendererinterface_p.h>
#include <chrono>

using namespace iShell;

class GStreamerRegression : public ::testing::Test {
protected:
    void SetUp() override {
        ASSERT_TRUE(gst_init_check(nullptr, nullptr, nullptr));
        GstElementFactory* factory = gst_element_factory_find("playbin");
        if (!factory) GTEST_SKIP() << "GStreamer playbin plugin unavailable";
        gst_object_unref(factory);
    }
};

class HeadlessRenderer : public iGstreamerVideoRendererInterface {
public:
    HeadlessRenderer() : sink(gst_element_factory_make("fakesink", nullptr)) {
        gst_object_ref_sink(sink);
    }
    ~HeadlessRenderer() override { gst_object_unref(sink); }
    GstElement* videoSink() override { return sink; }
    GstElement* sink;
};

TEST_F(GStreamerRegression, RemovingAbsentBusFilterIsIdempotent)
{
    GstBus* bus = gst_bus_new();
    {
        iGstreamerBusHelper helper(bus);
        iObject filter;
        helper.removeMessageFilter(&filter);
        helper.installMessageFilter(&filter);
        helper.removeMessageFilter(&filter);
        helper.removeMessageFilter(&filter);
    }
    gst_object_unref(bus);
}

TEST_F(GStreamerRegression, RendererCanDetachAfterCustomPipelineReplacement)
{
    HeadlessRenderer renderer;
    iGstreamerPlayerSession session(nullptr);
    session.setVideoRenderer(&renderer);
    session.loadFromUri(iUrl(iString("gst-pipeline:videotestsrc ! fakesink")));
    ASSERT_NE(nullptr, session.pipeline());
    session.setVideoRenderer(nullptr);
    EXPECT_EQ(nullptr, session.renderer());
}

TEST_F(GStreamerRegression, PlayingWithFullBufferIsNotStalled)
{
    iGstreamerPlayerSession session(nullptr);
    iGstreamerPlayerControl control(&session);
    control.setMedia(iUrl(iString("gst-pipeline:audiotestsrc is-live=true ! fakesink sync=false")), nullptr);
    control.play();
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (session.state() != iMediaPlayer::PlayingState && std::chrono::steady_clock::now() < deadline)
        iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
    ASSERT_EQ(iMediaPlayer::PlayingState, session.state());
    session.bufferingProgressChanged(100);
    EXPECT_EQ(iMediaPlayer::BufferedMedia, control.mediaStatus());
    control.stop();
}

TEST_F(GStreamerRegression, ActiveSinkSwapCompletesAndUnblocksIdlePad)
{
    const GstState states[] = {GST_STATE_PLAYING, GST_STATE_PAUSED};
    for (size_t index = 0; index < 2; ++index) {
        SCOPED_TRACE(states[index]);
        HeadlessRenderer first;
        HeadlessRenderer second;
        iGstreamerPlayerSession session(nullptr);
        session.setVideoRenderer(&first);
        GstObject* output = gst_object_get_parent(GST_OBJECT(first.sink));
        ASSERT_NE(nullptr, output);
        GstPad* ghost = gst_element_get_static_pad(GST_ELEMENT(output), "sink");
        GstPad* target = gst_ghost_pad_get_target(GST_GHOST_PAD(ghost));
        GstElement* identity = gst_pad_get_parent_element(target);
        GstPad* source = gst_element_get_static_pad(identity, "src");

        GstMessage* changed = gst_message_new_state_changed(GST_OBJECT(session.pipeline()),
            GST_STATE_PLAYING, states[index], GST_STATE_VOID_PENDING);
        session.processBusMessage(iGstreamerMessage(changed));
        gst_message_unref(changed);
        session.setVideoRenderer(&second);
        iCoreApplication::dispatchPostedEvents(nullptr, 0);
        GstObject* actualParent = gst_object_get_parent(GST_OBJECT(second.sink));
        EXPECT_EQ(output, actualParent);
        EXPECT_FALSE(gst_pad_is_blocked(source));
        if (actualParent) gst_object_unref(actualParent);
        gst_object_unref(source);
        gst_object_unref(identity);
        gst_object_unref(target);
        gst_object_unref(ghost);
        gst_object_unref(output);
    }
}