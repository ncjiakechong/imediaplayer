#include <gtest/gtest.h>
#include <core/kernel/icoreapplication.h>
#include <core/kernel/ieventdispatcher.h>
#include <multimedia/plugins/gstreamer/igstreamerbushelper_p.h>
#include <multimedia/plugins/gstreamer/igstreamerplayersession_p.h>
#include <multimedia/plugins/gstreamer/igstreamerplayercontrol_p.h>
#include <multimedia/plugins/gstreamer/igstreamervideorendererinterface_p.h>
#include <multimedia/plugins/gstreamer/igstvideopush_p.h>
#include <gst/app/gstappsink.h>
#include <gst/video/video.h>
#include <multimedia/codec/ivideoencoder.h>
#include <multimedia/codec/ivideodecoder.h>
#include <multimedia/video/ivideosink.h>
#include <multimedia/recording/imediarecorder.h>
#include <multimedia/playback/imediaplayer.h>
#include <chrono>
#include <cstring>
#include <vector>
#include <thread>
#include <glib/gstdio.h>

using namespace iShell;
using iShell::iMultimedia::SubmitResult;

class VideoResultObserver : public iObject {
    IX_OBJECT(VideoResultObserver)
public:
    VideoResultObserver() : errors(0), opened(0), closed(0), finished(0), ready(0), drained(0), success(false) {}
    void onFinalized(bool value) { ++finished; success = value; }
    void onPacket(iVideoAccessUnit unit) { packets.push_back(unit); }
    void onFrame(iVideoFrame frame, xuint64 frameId) {
        if (frame.isValid()) ids.push_back(frameId);
    }
    void onError(int, iString) { ++errors; }
    void onOpen(bool open) { open ? ++opened : ++closed; }
    void onRejected(xuint64 frameId, SubmitResult reason) { rejected.push_back(frameId); rejectionReasons.push_back(reason); }
    void onReady() { ++ready; }
    void onDrained() { ++drained; }
    void onVideoFrame(iVideoFrame frame) { if (frame.isValid()) frames.push_back(frame); }
    int errors;
    int opened;
    int closed;
    int finished;
    int ready;
    int drained;
    bool success;
    std::vector<xuint64> ids;
    std::vector<xuint64> rejected;
    std::vector<SubmitResult> rejectionReasons;
    std::vector<iVideoAccessUnit> packets;
    std::vector<iVideoFrame> frames;
};

static iVideoFrame solidVideoFrame()
{
    iVideoFrame frame(64 * 64 * 3, iSize(64, 64), 64 * 3, iVideoFrame::Format_BGR24);
    if (frame.map(iAbstractVideoBuffer::WriteOnly)) {
        std::memset(frame.bits(), 96, frame.mappedBytes());
        frame.unmap();
    }
    return frame;
}

static bool videoFactoryAvailable(const char* name)
{
    if (!gst_init_check(nullptr, nullptr, nullptr)) return false;
    GstElementFactory* factory = gst_element_factory_find(name);
    if (!factory) return false;
    gst_object_unref(factory);
    return true;
}

static std::vector<iVideoAccessUnit> makeH264AccessUnits(bool reordered)
{
    std::vector<iVideoAccessUnit> units;
    GError* error = nullptr;
    const iString description = iString::asprintf(
        "videotestsrc num-buffers=12 ! video/x-raw,format=I420,width=64,height=64,framerate=30/1 "
        "! x264enc bframes=%d b-adapt=false rc-lookahead=0 threads=1 key-int-max=30 "
        "! h264parse ! video/x-h264,stream-format=byte-stream,alignment=au ! appsink name=sink sync=false",
        reordered ? 2 : 0);
    GstElement* pipeline = gst_parse_launch(description.toUtf8().constData(), &error);
    if (error) g_error_free(error);
    if (!pipeline) return units;
    GstElement* sink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
    if (sink && gst_element_set_state(pipeline, GST_STATE_PLAYING) != GST_STATE_CHANGE_FAILURE) {
        while (units.size() < 12u) {
            GstSample* sample = gst_app_sink_try_pull_sample(GST_APP_SINK(sink), 3 * GST_SECOND);
            if (!sample) break;
            GstBuffer* buffer = gst_sample_get_buffer(sample);
            GstMapInfo map;
            if (buffer && gst_buffer_map(buffer, &map, GST_MAP_READ)) {
                iVideoAccessUnit unit;
                unit.data = iByteArray(reinterpret_cast<const char*>(map.data), map.size);
                unit.pts = gstSampleTimeUs(sample, GST_BUFFER_PTS(buffer));
                unit.dts = gstSampleTimeUs(sample, GST_BUFFER_DTS(buffer));
                unit.duration = GST_BUFFER_DURATION(buffer) / GST_USECOND;
                unit.frameId = static_cast<xuint64>((unit.pts * 30 + 500000) / 1000000);
                unit.keyFrame = !GST_BUFFER_FLAG_IS_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT);
                units.push_back(unit);
                gst_buffer_unmap(buffer, &map);
            }
            gst_sample_unref(sample);
        }
    }
    gst_element_set_state(pipeline, GST_STATE_NULL);
    if (sink) gst_object_unref(sink);
    gst_object_unref(pipeline);
    return units;
}

TEST(GStreamerVideoRegression, DecoderPreservesIdsAcrossBFrameReordering)
{
    if (!videoFactoryAvailable("x264enc") || !videoFactoryAvailable("avdec_h264"))
        GTEST_SKIP() << "H264 software codecs unavailable";
    const std::vector<iVideoAccessUnit> units = makeH264AccessUnits(true);
    ASSERT_EQ(12u, units.size());
    bool reordered = false;
    for (size_t index = 1; index < units.size(); ++index)
        if (units[index].frameId < units[index - 1].frameId) reordered = true;
    ASSERT_TRUE(reordered);

    VideoResultObserver observer;
    iVideoDecoder decoder;
    iObject::connect(&decoder, &iVideoDecoder::frameReady, &observer, &VideoResultObserver::onFrame);
    iObject::connect(&decoder, &iVideoDecoder::drained, &observer, &VideoResultObserver::onDrained);
    iObject::connect(&decoder, &iVideoDecoder::accessUnitRejected, &observer, &VideoResultObserver::onRejected);
    iVideoDecoderSettings settings;
    settings.codec = VideoCodec_H264;
    settings.maxBuffers = 32;
    settings.dropFrames = false;
    ASSERT_TRUE(decoder.open(settings));
    EXPECT_EQ(SubmitResult::InvalidInput, decoder.submitAccessUnit(iVideoAccessUnit()));
    ASSERT_EQ(1u, observer.rejectionReasons.size());
    EXPECT_EQ(SubmitResult::InvalidInput, observer.rejectionReasons.back());
    for (size_t index = 0; index < units.size(); ++index)
        ASSERT_EQ(SubmitResult::Accepted, decoder.submitAccessUnit(units[index]));
    ASSERT_TRUE(decoder.endOfStream());
    EXPECT_TRUE(decoder.isDraining());
    EXPECT_EQ(SubmitResult::Closed, decoder.submitAccessUnit(units.front()));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!observer.drained && std::chrono::steady_clock::now() < deadline)
        iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
    EXPECT_EQ(1, observer.drained);
    EXPECT_FALSE(decoder.isDraining());
    ASSERT_EQ(12u, observer.ids.size());
    for (size_t index = 0; index < observer.ids.size(); ++index)
        EXPECT_EQ(index, observer.ids[index]);
    decoder.close();
}

class VideoDeletionObserver : public iObject {
    IX_OBJECT(VideoDeletionObserver)
public:
    VideoDeletionObserver() : victim(nullptr), calls(0) {}
    ~VideoDeletionObserver() override { delete victim; }
    void destroy() {
        iObject* target = victim;
        victim = nullptr;
        ++calls;
        delete target;
    }
    void onPacket(iVideoAccessUnit) { destroy(); }
    void onFrame(iVideoFrame, xuint64) { destroy(); }
    void onOpen(bool open) { if (open) destroy(); }
    void onDrained() { destroy(); }
    void onFinalized(bool) { destroy(); }
    iObject* victim;
    int calls;
};

TEST(GStreamerVideoRegression, EncoderMayBeDeletedDuringOutputCallback)
{
    if (!videoFactoryAvailable("x264enc")) GTEST_SKIP() << "x264 unavailable";
    VideoDeletionObserver observer;
    iVideoEncoder* encoder = new iVideoEncoder;
    observer.victim = encoder;
    iObject::connect(encoder, &iVideoEncoder::packetReady, &observer, &VideoDeletionObserver::onPacket);
    iVideoEncoderSettings settings;
    settings.codec = VideoCodec_H264;
    ASSERT_TRUE(encoder->open(settings, iSize(64, 64)));
    for (xuint64 index = 0; index < 3; ++index) ASSERT_EQ(SubmitResult::Accepted, encoder->encode(solidVideoFrame(), index));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (observer.victim && std::chrono::steady_clock::now() < deadline)
        iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
    EXPECT_EQ(nullptr, observer.victim);
    EXPECT_EQ(1, observer.calls);
    iCoreApplication::dispatchPostedEvents(nullptr, 0);
}

TEST(GStreamerVideoRegression, DecoderMayBeDeletedDuringOutputCallback)
{
    if (!videoFactoryAvailable("x264enc") || !videoFactoryAvailable("avdec_h264"))
        GTEST_SKIP() << "H264 software codecs unavailable";
    const std::vector<iVideoAccessUnit> units = makeH264AccessUnits(false);
    ASSERT_EQ(12u, units.size());
    VideoDeletionObserver observer;
    iVideoDecoder* decoder = new iVideoDecoder;
    observer.victim = decoder;
    iObject::connect(decoder, &iVideoDecoder::frameReady, &observer, &VideoDeletionObserver::onFrame);
    iVideoDecoderSettings settings;
    settings.codec = VideoCodec_H264;
    ASSERT_TRUE(decoder->open(settings));
    for (size_t index = 0; index < units.size(); ++index) ASSERT_EQ(SubmitResult::Accepted, decoder->submitAccessUnit(units[index]));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (observer.victim && std::chrono::steady_clock::now() < deadline)
        iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
    EXPECT_EQ(nullptr, observer.victim);
    EXPECT_EQ(1, observer.calls);
    iCoreApplication::dispatchPostedEvents(nullptr, 0);
}

TEST(GStreamerVideoRegression, CodecMayBeDeletedDuringDrainedNotification)
{
    if (!videoFactoryAvailable("x264enc") || !videoFactoryAvailable("avdec_h264"))
        GTEST_SKIP() << "H264 software codecs unavailable";
    VideoDeletionObserver encoderObserver;
    iVideoEncoder* encoder = new iVideoEncoder;
    encoderObserver.victim = encoder;
    iObject::connect(encoder, &iVideoEncoder::drained, &encoderObserver, &VideoDeletionObserver::onDrained);
    iVideoEncoderSettings settings;
    settings.codec = VideoCodec_H264;
    ASSERT_TRUE(encoder->open(settings, iSize(64, 64)));
    ASSERT_EQ(SubmitResult::Accepted, encoder->encode(solidVideoFrame(), 1));
    ASSERT_TRUE(encoder->endOfStream());
    const auto encoderDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (encoderObserver.victim && std::chrono::steady_clock::now() < encoderDeadline)
        iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
    EXPECT_EQ(nullptr, encoderObserver.victim);
    EXPECT_EQ(1, encoderObserver.calls);

    VideoDeletionObserver decoderObserver;
    iVideoDecoder* decoder = new iVideoDecoder;
    decoderObserver.victim = decoder;
    iObject::connect(decoder, &iVideoDecoder::drained, &decoderObserver, &VideoDeletionObserver::onDrained);
    iVideoDecoderSettings decoderSettings;
    decoderSettings.codec = VideoCodec_H264;
    decoderSettings.dropFrames = false;
    ASSERT_TRUE(decoder->open(decoderSettings));
    const std::vector<iVideoAccessUnit> units = makeH264AccessUnits(true);
    ASSERT_EQ(12u, units.size());
    for (size_t index = 0; index < units.size(); ++index)
        ASSERT_EQ(SubmitResult::Accepted, decoder->submitAccessUnit(units[index]));
    ASSERT_TRUE(decoder->endOfStream());
    const auto decoderDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (decoderObserver.victim && std::chrono::steady_clock::now() < decoderDeadline)
        iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
    EXPECT_EQ(nullptr, decoderObserver.victim);
    EXPECT_EQ(1, decoderObserver.calls);
    iCoreApplication::dispatchPostedEvents(nullptr, 0);
}

TEST(GStreamerVideoRegression, VideoSinkIsAFrameEndpointWithoutOpeningARenderer)
{
    VideoResultObserver observer;
    iVideoSink sink;
    iObject::connect(&sink, &iVideoSink::videoFrameChanged, &observer, &VideoResultObserver::onVideoFrame);
    sink.setVideoFrame(solidVideoFrame());
    EXPECT_FALSE(sink.isOpen());
    EXPECT_TRUE(sink.videoFrame().isValid());
    EXPECT_EQ(iSize(64, 64), sink.frameSize());
    EXPECT_EQ(iSize(64, 64), sink.videoFrame().size());
    EXPECT_EQ(1u, observer.frames.size());
    sink.setVideoFrame(iVideoFrame());
    EXPECT_FALSE(sink.videoFrame().isValid());
}

TEST(GStreamerVideoRegression, SinkMayBeDeletedDuringOpenNotification)
{
    VideoDeletionObserver observer;
    iVideoSink* sink = new iVideoSink;
    observer.victim = sink;
    iObject::connect(sink, &iVideoSink::openChanged, &observer, &VideoDeletionObserver::onOpen);
    EXPECT_FALSE(sink->open(iString("fakesink"), iSize(64, 64)));
    EXPECT_EQ(nullptr, observer.victim);
    EXPECT_EQ(1, observer.calls);
}

TEST(GStreamerVideoRegression, RecordingAdapterReportsRejectedFrame)
{
    VideoResultObserver observer;
    iMediaRecorder recorder;
    iObject::connect(&recorder, &iMediaRecorder::frameRejected, &observer, &VideoResultObserver::onRejected);
    EXPECT_EQ(SubmitResult::Closed, recorder.writeFrame(solidVideoFrame(), 73));
    ASSERT_EQ(1u, observer.rejected.size());
    EXPECT_EQ(73u, observer.rejected.front());
    EXPECT_EQ(0u, recorder.framesAccepted());
    iObject::invokeMethod(&recorder, &iMediaRecorder::writeFrame, solidVideoFrame(), xuint64(74), QueuedConnection);
    EXPECT_EQ(1u, observer.rejected.size());
    iCoreApplication::dispatchPostedEvents(nullptr, 0);
    ASSERT_EQ(2u, observer.rejected.size());
    EXPECT_EQ(74u, observer.rejected.back());
    ASSERT_EQ(2u, observer.rejectionReasons.size());
    EXPECT_EQ(SubmitResult::Closed, observer.rejectionReasons.back());
    iMediaRecorderSettings settings;
    settings.encoderChain = iString("fakesink sync=false");
    ASSERT_TRUE(recorder.open(settings, iSize(64, 64)));
    iObject::invokeMethod(&recorder, &iMediaRecorder::writeFrame, iVideoFrame(), xuint64(75), QueuedConnection);
    iCoreApplication::dispatchPostedEvents(nullptr, 0);
    ASSERT_EQ(3u, observer.rejectionReasons.size());
    EXPECT_EQ(75u, observer.rejected.back());
    EXPECT_EQ(SubmitResult::InvalidInput, observer.rejectionReasons.back());
    EXPECT_EQ(0u, recorder.framesAccepted());
    EXPECT_EQ(SubmitResult::Accepted, recorder.write(solidVideoFrame()));
    EXPECT_TRUE(recorder.finalize(1000));
}

TEST(GStreamerVideoRegression, RecordingStopIsAsynchronousAndRejectsNewFrames)
{
    VideoResultObserver observer;
    iMediaRecorder recorder;
    iObject::connect(&recorder, &iMediaRecorder::finalized, &observer, &VideoResultObserver::onFinalized);
    iObject::connect(&recorder, &iMediaRecorder::error, &observer, &VideoResultObserver::onError);
    iMediaRecorderSettings settings;
    settings.encoderChain = iString("identity sleep-time=100000 ! fakesink sync=false");
    ASSERT_TRUE(recorder.open(settings, iSize(64, 64)));
    for (int index = 0; index < 3; ++index) ASSERT_EQ(SubmitResult::Accepted, recorder.write(solidVideoFrame()));
    const auto started = std::chrono::steady_clock::now();
    ASSERT_TRUE(recorder.stop(2000));
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - started).count(), 150);
    EXPECT_TRUE(recorder.isFinalizing());
    EXPECT_EQ(SubmitResult::Closed, recorder.write(solidVideoFrame()));
    EXPECT_TRUE(recorder.stop(2000));
    EXPECT_FALSE(recorder.finalize(-1));
    EXPECT_EQ(1, observer.errors);
    iObject::invokeMethod(&observer, &VideoResultObserver::onReady, QueuedConnection);
    iCoreApplication::dispatchPostedEvents(nullptr, 0);
    EXPECT_EQ(1, observer.ready);
    EXPECT_EQ(0, observer.finished);
    const auto deadline = started + std::chrono::seconds(4);
    while (!observer.finished && std::chrono::steady_clock::now() < deadline)
        iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
    EXPECT_EQ(1, observer.finished);
    EXPECT_TRUE(observer.success);
    EXPECT_EQ(1, observer.errors);
    EXPECT_TRUE(recorder.errorString().isEmpty());
    EXPECT_FALSE(recorder.isOpen());
    EXPECT_FALSE(recorder.isFinalizing());
    EXPECT_EQ(3u, recorder.framesAccepted());
}

TEST(GStreamerVideoRegression, RecordingAsyncStopReportsTimeout)
{
    VideoResultObserver observer;
    iMediaRecorder recorder;
    iObject::connect(&recorder, &iMediaRecorder::finalized, &observer, &VideoResultObserver::onFinalized);
    iObject::connect(&recorder, &iMediaRecorder::error, &observer, &VideoResultObserver::onError);
    iMediaRecorderSettings settings;
    settings.encoderChain = iString("valve drop=true ! fakesink sync=false");
    ASSERT_TRUE(recorder.open(settings, iSize(64, 64)));
    ASSERT_EQ(SubmitResult::Accepted, recorder.write(solidVideoFrame()));
    ASSERT_TRUE(recorder.stop(1));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!observer.finished && std::chrono::steady_clock::now() < deadline)
        iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
    EXPECT_EQ(1, observer.finished);
    EXPECT_FALSE(observer.success);
    EXPECT_EQ(1, observer.errors);
    EXPECT_FALSE(recorder.isFinalizing());
    EXPECT_FALSE(recorder.isOpen());
    EXPECT_TRUE(recorder.errorString().contains(iString("timed out")));
}

TEST(GStreamerVideoRegression, RecorderMayBeDeletedDuringAsyncFinalization)
{
    for (int duringStart = 0; duringStart < 2; ++duringStart) {
        VideoDeletionObserver observer;
        iMediaRecorder* recorder = new iMediaRecorder;
        observer.victim = recorder;
        iMediaRecorderSettings settings;
        settings.encoderChain = iString("fakesink sync=false");
        ASSERT_TRUE(recorder->open(settings, iSize(64, 64)));
        ASSERT_EQ(SubmitResult::Accepted, recorder->write(solidVideoFrame()));
        if (duringStart)
            iObject::connect(recorder, &iMediaRecorder::finalizingChanged, &observer, &VideoDeletionObserver::onOpen);
        else
            iObject::connect(recorder, &iMediaRecorder::finalized, &observer, &VideoDeletionObserver::onFinalized);
        recorder->close();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (observer.victim && std::chrono::steady_clock::now() < deadline)
            iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
        EXPECT_EQ(nullptr, observer.victim);
        EXPECT_EQ(1, observer.calls);
        iCoreApplication::dispatchPostedEvents(nullptr, 0);
    }
}

class RecorderReopenObserver : public iObject {
    IX_OBJECT(RecorderReopenObserver)
public:
    explicit RecorderReopenObserver(iMediaRecorder* target) : recorder(target), reopened(false), armed(true) {}
    void onFinalized(bool success) {
        if (!success || !armed) return;
        armed = false;
        iMediaRecorderSettings settings;
        settings.encoderChain = iString("fakesink sync=false");
        reopened = recorder->open(settings, iSize(64, 64));
    }
    iMediaRecorder* recorder;
    bool reopened;
    bool armed;
};

TEST(GStreamerVideoRegression, RecorderCanReopenFromAsyncCompletion)
{
    iMediaRecorder recorder;
    RecorderReopenObserver observer(&recorder);
    iObject::connect(&recorder, &iMediaRecorder::finalized, &observer, &RecorderReopenObserver::onFinalized);
    iMediaRecorderSettings settings;
    settings.encoderChain = iString("fakesink sync=false");
    ASSERT_TRUE(recorder.open(settings, iSize(64, 64)));
    ASSERT_EQ(SubmitResult::Accepted, recorder.write(solidVideoFrame()));
    ASSERT_TRUE(recorder.stop(1000));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!observer.reopened && std::chrono::steady_clock::now() < deadline)
        iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
    EXPECT_TRUE(observer.reopened);
    EXPECT_TRUE(recorder.isOpen());
    EXPECT_FALSE(recorder.isFinalizing());
    EXPECT_EQ(0u, recorder.framesAccepted());
    EXPECT_EQ(SubmitResult::Accepted, recorder.write(solidVideoFrame()));
    EXPECT_TRUE(recorder.finalize(1000));
}

TEST(GStreamerVideoRegression, RecordingFinalizationReportsTimeout)
{
    VideoResultObserver observer;
    iMediaRecorder recorder;
    iObject::connect(&recorder, &iMediaRecorder::error, &observer, &VideoResultObserver::onError);
    iMediaRecorderSettings settings;
    settings.encoderChain = iString("valve drop=true ! fakesink sync=false");
    ASSERT_TRUE(recorder.open(settings, iSize(64, 64)));
    ASSERT_EQ(SubmitResult::Accepted, recorder.write(solidVideoFrame()));
    EXPECT_FALSE(recorder.finalize(1));
    EXPECT_TRUE(recorder.errorString().contains(iString("timed out")));
    EXPECT_FALSE(recorder.isOpen());
    EXPECT_EQ(1, observer.errors);
}

TEST(GStreamerVideoRegression, RecorderRejectsUnsupportedContainersBeforeCreatingFiles)
{
    gchar* directory = g_dir_make_tmp("ix-container-XXXXXX", nullptr);
    ASSERT_NE(nullptr, directory);
    VideoResultObserver observer;
    iMediaRecorder recorder;
    iObject::connect(&recorder, &iMediaRecorder::error, &observer, &VideoResultObserver::onError);
    const char* names[] = {"output.webm", "output.unknown", "output"};
    iMediaRecorderSettings settings;
    for (size_t index = 0; index < 3; ++index) {
        settings.path = iString::asprintf("%s/%s", directory, names[index]);
        EXPECT_FALSE(recorder.open(settings, iSize(64, 64)));
        EXPECT_FALSE(recorder.isOpen());
        EXPECT_TRUE(recorder.errorString().contains(iString("unsupported recording container")));
        EXPECT_FALSE(g_file_test(settings.path.toUtf8().constData(), G_FILE_TEST_EXISTS));
        if (recorder.isOpen()) recorder.finalize(1000);
        g_remove(settings.path.toUtf8().constData());
    }
    EXPECT_EQ(3, observer.errors);
    settings.encoderChain = iString("fakesink sync=false");
    EXPECT_TRUE(recorder.open(settings, iSize(64, 64)));
    EXPECT_EQ(SubmitResult::Accepted, recorder.write(solidVideoFrame()));
    EXPECT_TRUE(recorder.finalize(1000));
    EXPECT_FALSE(g_file_test(settings.path.toUtf8().constData(), G_FILE_TEST_EXISTS));
    g_rmdir(directory);
    g_free(directory);
}

TEST(GStreamerVideoRegression, FinalizedMp4CanBeDecoded)
{
    if (!videoFactoryAvailable("x264enc") || !videoFactoryAvailable("avdec_h264"))
        GTEST_SKIP() << "H264 software codecs unavailable";
    gchar* directory = g_dir_make_tmp("ix-media-XXXXXX", nullptr);
    ASSERT_NE(nullptr, directory);
    const iString filename = iString::asprintf("%s/quoted\"file.mp4", directory);
    iMediaRecorder recorder;
    iMediaRecorderSettings settings;
    settings.path = filename;
    const bool opened = recorder.open(settings, iSize(64, 64));
    EXPECT_TRUE(opened);
    if (opened) {
        const xint64 times[] = {0, 2000000, 2040000};
        for (int index = 0; index < 3; ++index) {
            iVideoFrame frame = solidVideoFrame();
            frame.setStartTime(times[index]);
            frame.setEndTime(times[index] + 40000);
            EXPECT_EQ(SubmitResult::Accepted, recorder.write(frame));
        }
        EXPECT_TRUE(recorder.finalize(3000));
        gchar* uri = gst_filename_to_uri(filename.toUtf8().constData(), nullptr);
        GstElement* pipeline = gst_parse_launch(iString::asprintf(
            "uridecodebin uri=\"%s\" ! videoconvert ! appsink name=sink sync=false", uri).toUtf8().constData(), nullptr);
        g_free(uri);
        EXPECT_NE(nullptr, pipeline);
        if (pipeline) {
            GstElement* sink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
            gst_element_set_state(pipeline, GST_STATE_PLAYING);
            int decoded = 0;
            GstSample* sample;
            while ((sample = gst_app_sink_try_pull_sample(GST_APP_SINK(sink), 3 * GST_SECOND)) != nullptr) {
                if (decoded < 3)
                    EXPECT_NEAR(times[decoded], gstSampleTimeUs(sample, GST_BUFFER_PTS(gst_sample_get_buffer(sample))), 1000);
                ++decoded;
                gst_sample_unref(sample);
            }
            EXPECT_EQ(3, decoded);
            gst_element_set_state(pipeline, GST_STATE_NULL);
            gst_object_unref(sink);
            gst_object_unref(pipeline);
        }
    }
    if (opened) {
        VideoResultObserver observer;
        VideoResultObserver replacementObserver;
        iVideoSink sink;
        iVideoSink replacement;
        iMediaPlayer player;
        iObject::connect(&sink, &iVideoSink::videoFrameChanged, &observer, &VideoResultObserver::onVideoFrame);
        player.setVideoOutput(&sink);
        gchar* uri = gst_filename_to_uri(filename.toUtf8().constData(), nullptr);
        player.setMedia(iUrl(iString::fromUtf8(uri, -1)));
        g_free(uri);
        player.play();
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
        while (observer.frames.empty() && std::chrono::steady_clock::now() < deadline)
            iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
        EXPECT_FALSE(observer.frames.empty());
        EXPECT_EQ(iSize(64, 64), sink.videoFrame().size());
        EXPECT_EQ(iVideoFrame::Format_BGR24, sink.videoFrame().pixelFormat());
        iObject::connect(&replacement, &iVideoSink::videoFrameChanged,
                        &replacementObserver, &VideoResultObserver::onVideoFrame);
        player.setVideoOutput(&replacement);
        const auto swapDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(4);
        while (replacementObserver.frames.empty() && std::chrono::steady_clock::now() < swapDeadline)
            iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
        EXPECT_FALSE(replacementObserver.frames.empty());
        player.stop();
        player.setVideoOutput(nullptr);
    }
    g_remove(filename.toUtf8().constData());
    g_rmdir(directory);
    g_free(directory);
}

TEST(GStreamerVideoRegression, RecordingFinalizationReportsErrorAndAcceptedCount)
{
    VideoResultObserver observer;
    iMediaRecorder recorder;
    iObject::connect(&recorder, &iMediaRecorder::error, &observer, &VideoResultObserver::onError);
    iObject::connect(&recorder, &iMediaRecorder::finalized, &observer, &VideoResultObserver::onFinalized);
    iMediaRecorderSettings settings;
    settings.encoderChain = iString("identity error-after=1 ! fakesink sync=false");
    ASSERT_TRUE(recorder.open(settings, iSize(64, 64)));
    ASSERT_EQ(SubmitResult::Accepted, recorder.write(solidVideoFrame()));
    EXPECT_FALSE(recorder.finalize(1000));
    EXPECT_EQ(1u, recorder.framesAccepted());
    EXPECT_FALSE(recorder.isOpen());
    EXPECT_FALSE(recorder.errorString().isEmpty());
    EXPECT_EQ(1, observer.finished);
    EXPECT_FALSE(observer.success);
    EXPECT_EQ(1, observer.errors);
    EXPECT_FALSE(recorder.finalize(1000));
    EXPECT_EQ(1, observer.finished);
}

TEST(GStreamerVideoRegression, RecordingFinalizationWaitsForEos)
{
    VideoResultObserver observer;
    iMediaRecorder recorder;
    iObject::connect(&recorder, &iMediaRecorder::finalized, &observer, &VideoResultObserver::onFinalized);
    iMediaRecorderSettings settings;
    settings.encoderChain = iString("fakesink sync=false");
    ASSERT_TRUE(recorder.open(settings, iSize(64, 64)));
    ASSERT_EQ(SubmitResult::Accepted, recorder.write(solidVideoFrame()));
    EXPECT_TRUE(recorder.finalize(1000));
    EXPECT_TRUE(recorder.errorString().isEmpty());
    EXPECT_EQ(1, observer.finished);
    EXPECT_TRUE(observer.success);
    EXPECT_TRUE(recorder.finalize(1000));
    EXPECT_EQ(1, observer.finished);
}

TEST(GStreamerVideoRegression, RecordingRuntimeFailureStopsSession)
{
    VideoResultObserver observer;
    iMediaRecorder recorder;
    iObject::connect(&recorder, &iMediaRecorder::error, &observer, &VideoResultObserver::onError);
    iObject::connect(&recorder, &iMediaRecorder::finalized, &observer, &VideoResultObserver::onFinalized);
    iMediaRecorderSettings settings;
    settings.encoderChain = iString(
        "tee name=split ! queue ! identity error-after=1 ! fakesink sync=false async=false "
        "split. ! queue ! identity sleep-time=1000000 ! fakesink sync=false async=false");
    ASSERT_TRUE(recorder.open(settings, iSize(64, 64)));
    ASSERT_EQ(SubmitResult::Accepted, recorder.write(solidVideoFrame()));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    std::chrono::steady_clock::duration longestDispatch = std::chrono::steady_clock::duration::zero();
    while (!observer.errors && std::chrono::steady_clock::now() < deadline) {
        const auto before = std::chrono::steady_clock::now();
        iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
        const auto elapsed = std::chrono::steady_clock::now() - before;
        if (elapsed > longestDispatch) longestDispatch = elapsed;
    }
    EXPECT_LT(std::chrono::duration_cast<std::chrono::milliseconds>(longestDispatch).count(), 250);
    EXPECT_EQ(1, observer.errors);
    EXPECT_EQ(1, observer.finished);
    EXPECT_FALSE(observer.success);
    EXPECT_FALSE(recorder.isOpen());
    EXPECT_FALSE(recorder.isFinalizing());
    EXPECT_FALSE(recorder.errorString().isEmpty());
    EXPECT_FALSE(recorder.errorString().contains(iString("timed out")));
    EXPECT_FALSE(recorder.finalize());
}

TEST(GStreamerVideoRegression, SinkReportsRuntimeErrorAndCloses)
{
    VideoResultObserver observer;
    iVideoSink sink;
    iObject::connect(&sink, &iVideoSink::error, &observer, &VideoResultObserver::onError);
    iObject::connect(&sink, &iVideoSink::openChanged, &observer, &VideoResultObserver::onOpen);
    ASSERT_TRUE(sink.open(iString("identity error-after=1 ! fakesink"), iSize(64, 64)));
    ASSERT_TRUE(sink.present(solidVideoFrame()));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (observer.errors == 0 && std::chrono::steady_clock::now() < deadline)
        iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
    EXPECT_EQ(1, observer.errors);
    EXPECT_FALSE(sink.errorString().isEmpty());
    EXPECT_FALSE(sink.isOpen());
    EXPECT_EQ(1, observer.opened);
    EXPECT_EQ(1, observer.closed);
}

TEST(GStreamerVideoRegression, SinkFailedReopenNotifiesClosed)
{
    VideoResultObserver observer;
    iVideoSink sink;
    iObject::connect(&sink, &iVideoSink::openChanged, &observer, &VideoResultObserver::onOpen);
    ASSERT_TRUE(sink.open(iString("fakesink"), iSize(64, 64)));
    EXPECT_FALSE(sink.open(iString("fakesink"), iSize(0, 0)));
    EXPECT_FALSE(sink.isOpen());
    EXPECT_EQ(1, observer.opened);
    EXPECT_EQ(1, observer.closed);
}

TEST(GStreamerVideoRegression, CodecFailedReopenNotifiesClosed)
{
    if (!videoFactoryAvailable("x264enc") || !videoFactoryAvailable("avdec_h264"))
        GTEST_SKIP() << "H264 software codecs unavailable";
    VideoResultObserver encoderObserver;
    iVideoEncoder encoder;
    iObject::connect(&encoder, &iVideoEncoder::openChanged, &encoderObserver, &VideoResultObserver::onOpen);
    iVideoEncoderSettings encoderSettings;
    encoderSettings.codec = VideoCodec_H264;
    ASSERT_TRUE(encoder.open(encoderSettings, iSize(64, 64)));
    encoderSettings.framerate = 0;
    EXPECT_FALSE(encoder.open(encoderSettings, iSize(64, 64)));
    EXPECT_FALSE(encoder.isOpen());
    EXPECT_EQ(1, encoderObserver.closed);

    VideoResultObserver decoderObserver;
    iVideoDecoder decoder;
    iObject::connect(&decoder, &iVideoDecoder::openChanged, &decoderObserver, &VideoResultObserver::onOpen);
    iVideoDecoderSettings decoderSettings;
    decoderSettings.codec = VideoCodec_H264;
    ASSERT_TRUE(decoder.open(decoderSettings));
    decoderSettings.framerate = 0;
    EXPECT_FALSE(decoder.open(decoderSettings));
    EXPECT_FALSE(decoder.isOpen());
    EXPECT_EQ(1, decoderObserver.closed);
}

TEST(GStreamerVideoRegression, CrossThreadSubmissionIsRejected)
{
    iVideoSink sink;
    ASSERT_TRUE(sink.open(iString("fakesink"), iSize(64, 64)));
    iVideoFrame frame = solidVideoFrame();
    bool accepted = true;
    std::thread producer([&]() { accepted = sink.present(frame); });
    producer.join();
    EXPECT_FALSE(accepted);
    EXPECT_TRUE(sink.isOpen());
    EXPECT_TRUE(sink.present(frame));
    sink.close();
}

class SinkReopenObserver : public iObject {
    IX_OBJECT(SinkReopenObserver)
public:
    explicit SinkReopenObserver(iVideoSink* target) : sink(target), armed(true), reopened(false), errors(0) {}
    void onOpen(bool open) {
        if (open || !armed) return;
        armed = false;
        reopened = sink->open(iString("fakesink"), iSize(64, 64));
    }
    void onError(int, iString) { ++errors; }
    iVideoSink* sink;
    bool armed;
    bool reopened;
    int errors;
};

TEST(GStreamerVideoRegression, ReopenFromErrorClosePreservesNewSession)
{
    iVideoSink sink;
    SinkReopenObserver observer(&sink);
    iObject::connect(&sink, &iVideoSink::openChanged, &observer, &SinkReopenObserver::onOpen);
    iObject::connect(&sink, &iVideoSink::error, &observer, &SinkReopenObserver::onError);
    ASSERT_TRUE(sink.open(iString("identity error-after=1 ! fakesink"), iSize(64, 64)));
    ASSERT_TRUE(sink.present(solidVideoFrame()));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!observer.reopened && std::chrono::steady_clock::now() < deadline)
        iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
    EXPECT_TRUE(observer.reopened);
    EXPECT_TRUE(sink.isOpen());
    EXPECT_TRUE(sink.errorString().isEmpty());
    EXPECT_EQ(0, observer.errors);
    EXPECT_TRUE(sink.present(solidVideoFrame()));
    sink.close();
}

TEST(GStreamerVideoRegression, InputReadinessResumesAfterBackpressure)
{
    ASSERT_TRUE(gst_init_check(nullptr, nullptr, nullptr));
    GstElement* pipeline = nullptr;
    GstAppSrc* appsrc = nullptr;
    iString error;
    ASSERT_TRUE(gstBuildPushPipeline(iString(
        "appsrc name=src is-live=true block=false max-buffers=2 max-bytes=0 format=time "
        "caps=video/x-raw,format=BGR,width=64,height=64,framerate=30/1 ! fakesink sync=false async=false"),
        "src", &pipeline, &appsrc, &error));
    GstPad* source = gst_element_get_static_pad(GST_ELEMENT(appsrc), "src");
    const gulong blocker = gst_pad_add_probe(source, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
        +[](GstPad*, GstPadProbeInfo*, gpointer) -> GstPadProbeReturn { return GST_PAD_PROBE_OK; }, nullptr, nullptr);
    VideoResultObserver observer;
    iGstVideoInput* input = new iGstVideoInput(appsrc);
    iObject::connect(input, &iGstVideoInput::readyToSubmit, &observer, &VideoResultObserver::onReady);
    const iVideoFrame frame = solidVideoFrame();
    int accepted = 0;
    while (accepted < 8 && input->checkFrame(frame) == SubmitResult::Accepted) {
        EXPECT_EQ(SubmitResult::Accepted, gstPushBgrFrame(appsrc, frame, accepted * (GST_SECOND / 30), GST_SECOND / 30));
        ++accepted;
    }
    EXPECT_LT(accepted, 8);
    iCoreApplication::dispatchPostedEvents(nullptr, 0);
    EXPECT_EQ(0, observer.ready);
    gst_pad_remove_probe(source, blocker);
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!observer.ready && std::chrono::steady_clock::now() < deadline)
        iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
    EXPECT_EQ(1, observer.ready);
    EXPECT_EQ(SubmitResult::Accepted, input->checkFrame(frame));
    EXPECT_EQ(SubmitResult::InvalidInput, input->checkCapacity(0));
    gst_app_src_set_max_bytes(appsrc, 1024);
    EXPECT_EQ(SubmitResult::InvalidInput, input->checkFrame(frame));
    gst_app_src_set_max_bytes(appsrc, 0);
    input->setCapacityAvailable(false);
    EXPECT_EQ(SubmitResult::WouldBlock, input->checkFrame(frame));
    input->setCapacityAvailable(true);
    input->stop();
    EXPECT_EQ(SubmitResult::Closed, input->checkFrame(frame));
    iCoreApplication::dispatchPostedEvents(nullptr, 0);
    EXPECT_EQ(1, observer.ready);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    delete input;
    gst_object_unref(source);
    gst_object_unref(appsrc);
    gst_object_unref(pipeline);
}

TEST(GStreamerVideoRegression, SinkAppsrcQueueIsBounded)
{
    iVideoSink sink;
    ASSERT_TRUE(sink.open(iString("fakesink"), iSize(64, 64)));
    const iString description = sink.description();
    sink.close();
    GstElement* pipeline = nullptr;
    GstAppSrc* appsrc = nullptr;
    iString error;
    ASSERT_TRUE(gstBuildPushPipeline(description, "rendersrc", &pipeline, &appsrc, &error));
    GstPad* source = gst_element_get_static_pad(GST_ELEMENT(appsrc), "src");
    const gulong blocker = gst_pad_add_probe(source, GST_PAD_PROBE_TYPE_BLOCK_DOWNSTREAM,
        +[](GstPad*, GstPadProbeInfo*, gpointer) -> GstPadProbeReturn { return GST_PAD_PROBE_OK; }, nullptr, nullptr);
    iVideoFrame frame = solidVideoFrame();
    for (int index = 0; index < 100; ++index)
        EXPECT_EQ(SubmitResult::Accepted, gstPushBgrFrame(appsrc, frame, index * (GST_SECOND / 30), GST_SECOND / 30));
    EXPECT_LE(gst_app_src_get_current_level_buffers(appsrc), 4u);
    EXPECT_LE(gst_app_src_get_current_level_bytes(appsrc), 4u * 64u * 64u * 3u);
    gst_pad_remove_probe(source, blocker);
    gst_object_unref(source);
    gst_element_set_state(pipeline, GST_STATE_NULL);
    gst_object_unref(appsrc);
    gst_object_unref(pipeline);
}

TEST(GStreamerVideoRegression, AllIntraViolationFailsInsteadOfReportingDrained)
{
    if (!videoFactoryAvailable("x264enc")) GTEST_SKIP() << "x264 unavailable";
    VideoResultObserver observer;
    iVideoEncoder encoder;
    iObject::connect(&encoder, &iVideoEncoder::packetReady, &observer, &VideoResultObserver::onPacket);
    iObject::connect(&encoder, &iVideoEncoder::drained, &observer, &VideoResultObserver::onDrained);
    iObject::connect(&encoder, &iVideoEncoder::error, &observer, &VideoResultObserver::onError);
    iObject::connect(&encoder, &iVideoEncoder::openChanged, &observer, &VideoResultObserver::onOpen);
    int attached = 0;
    const guint signal = g_signal_lookup("element-added", GST_TYPE_BIN);
    ASSERT_NE(0u, signal);
    const gulong hook = g_signal_add_emission_hook(signal, 0,
        +[](GSignalInvocationHint*, guint count, const GValue* values, gpointer data) -> gboolean {
            if (count < 2) return TRUE;
            GstElement* element = GST_ELEMENT(g_value_get_object(&values[1]));
            if (!GST_IS_APP_SINK(element) || g_strcmp0(GST_OBJECT_NAME(element), "encsink") != 0) return TRUE;
            GstPad* pad = gst_element_get_static_pad(element, "sink");
            const gulong probe = gst_pad_add_probe(pad, GST_PAD_PROBE_TYPE_BUFFER,
                +[](GstPad*, GstPadProbeInfo* info, gpointer) -> GstPadProbeReturn {
                    GstBuffer* buffer = gst_buffer_make_writable(GST_PAD_PROBE_INFO_BUFFER(info));
                    GST_PAD_PROBE_INFO_DATA(info) = buffer;
                    GST_BUFFER_FLAG_SET(buffer, GST_BUFFER_FLAG_DELTA_UNIT);
                    return GST_PAD_PROBE_OK;
                }, nullptr, nullptr);
            if (probe) ++*static_cast<int*>(data);
            gst_object_unref(pad);
            return TRUE;
        }, &attached, nullptr);
    ASSERT_NE(0u, hook);
    iVideoEncoderSettings settings;
    settings.codec = VideoCodec_H264;
    const bool opened = encoder.open(settings, iSize(64, 64));
    g_signal_remove_emission_hook(signal, hook);
    ASSERT_TRUE(opened);
    ASSERT_GT(attached, 0);
    ASSERT_EQ(SubmitResult::Accepted, encoder.encode(solidVideoFrame(), 37));
    ASSERT_TRUE(encoder.endOfStream());
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!observer.errors && !observer.drained && std::chrono::steady_clock::now() < deadline)
        iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
    EXPECT_EQ(1, observer.errors);
    EXPECT_EQ(1, observer.closed);
    EXPECT_EQ(0, observer.drained);
    EXPECT_TRUE(observer.packets.empty());
    EXPECT_FALSE(encoder.isOpen());
    EXPECT_TRUE(encoder.errorString().contains(iString("all-intra")));
}

TEST(GStreamerVideoRegression, InterFrameEncodersDrainEveryAcceptedFrame)
{
    const char* factories[] = {"x264enc", "x265enc"};
    int exercised = 0;
    for (int codec = 0; codec < 2; ++codec) {
        if (!videoFactoryAvailable(factories[codec])) continue;
        ++exercised;
        SCOPED_TRACE(factories[codec]);
        VideoResultObserver observer;
        iVideoEncoder encoder;
        iObject::connect(&encoder, &iVideoEncoder::packetReady, &observer, &VideoResultObserver::onPacket);
        iObject::connect(&encoder, &iVideoEncoder::drained, &observer, &VideoResultObserver::onDrained);
        iObject::connect(&encoder, &iVideoEncoder::error, &observer, &VideoResultObserver::onError);
        iVideoEncoderSettings settings;
        settings.codec = static_cast<iVideoCodec>(codec);
        settings.allIntra = false;
        ASSERT_TRUE(encoder.open(settings, iSize(64, 64)));
        const iVideoFrame frame = solidVideoFrame();
        xuint64 accepted = 0;
        const auto submitDeadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (accepted < 96 && std::chrono::steady_clock::now() < submitDeadline) {
            if (encoder.encode(frame, accepted) == SubmitResult::Accepted) ++accepted;
            iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
        }
        ASSERT_EQ(96u, accepted);
        ASSERT_TRUE(encoder.endOfStream());
        EXPECT_TRUE(encoder.endOfStream());
        EXPECT_EQ(SubmitResult::Closed, encoder.encode(frame, accepted));
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);
        while (!observer.drained && !observer.errors && std::chrono::steady_clock::now() < deadline)
            iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
        EXPECT_EQ(0, observer.errors);
        EXPECT_EQ(1, observer.drained);
        EXPECT_FALSE(encoder.isDraining());
        ASSERT_EQ(96u, observer.packets.size());
        std::vector<bool> seen(96, false);
        for (size_t index = 0; index < observer.packets.size(); ++index) {
            const xuint64 frameId = observer.packets[index].frameId;
            ASSERT_LT(frameId, seen.size());
            EXPECT_FALSE(seen[frameId]);
            seen[frameId] = true;
        }
        encoder.close();
    }
    if (!exercised) GTEST_SKIP() << "Software encoders unavailable";
}

TEST(GStreamerVideoRegression, EncoderPreservesTimesUnlessFixedRateIsRequested)
{
    if (!videoFactoryAvailable("x264enc")) GTEST_SKIP() << "x264 unavailable";
    for (int fixed = 0; fixed < 2; ++fixed) {
        VideoResultObserver observer;
        iVideoEncoder encoder;
        iObject::connect(&encoder, &iVideoEncoder::packetReady, &observer, &VideoResultObserver::onPacket);
        iVideoEncoderSettings settings;
        settings.codec = VideoCodec_H264;
        settings.timestampPolicy = fixed ? VideoTimestamp_FixedRate : VideoTimestamp_Preserve;
        ASSERT_TRUE(encoder.open(settings, iSize(64, 64)));
        for (int index = 0; index < 2; ++index) {
            iVideoFrame frame = solidVideoFrame();
            frame.setStartTime(index * 2000000);
            frame.setEndTime(frame.startTime() + 40000);
            ASSERT_EQ(SubmitResult::Accepted, encoder.encode(frame, 101 + index));
        }
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
        while (observer.packets.size() < 2u && std::chrono::steady_clock::now() < deadline)
            iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
        ASSERT_EQ(2u, observer.packets.size());
        EXPECT_EQ(0, observer.packets[0].pts);
        EXPECT_EQ(fixed ? 33333 : 2000000, observer.packets[1].pts);
        EXPECT_EQ(fixed ? 33333 : 40000, observer.packets[1].duration);
        EXPECT_EQ(102u, observer.packets[1].frameId);
        encoder.close();
    }
}

TEST(GStreamerVideoRegression, FrameTimingRejectsInvalidAndBackwardTimes)
{
    iVideoFrame frame = solidVideoFrame();
    GstClockTime pts;
    GstClockTime duration;
    EXPECT_TRUE(gstFrameTiming(frame, VideoTimestamp_Preserve, GST_SECOND / 25, 2 * GST_SECOND, GST_SECOND, &pts, &duration));
    EXPECT_EQ(2 * GST_SECOND, pts);
    EXPECT_EQ(GST_SECOND / 25, duration);
    frame.setStartTime(1000000);
    EXPECT_FALSE(gstFrameTiming(frame, VideoTimestamp_Preserve, GST_SECOND / 25, 2 * GST_SECOND, GST_SECOND, &pts, &duration));
    frame.setStartTime(2000000);
    frame.setEndTime(1999999);
    EXPECT_FALSE(gstFrameTiming(frame, VideoTimestamp_Preserve, GST_SECOND / 25, 2 * GST_SECOND, GST_SECOND, &pts, &duration));
    frame.setStartTime(-2);
    EXPECT_FALSE(gstFrameTiming(frame, VideoTimestamp_Preserve, GST_SECOND / 25, 0, GST_CLOCK_TIME_NONE, &pts, &duration));
}

TEST(GStreamerVideoRegression, EncoderReportsCapacityRecovery)
{
    if (!videoFactoryAvailable("x264enc")) GTEST_SKIP() << "x264 unavailable";
    VideoResultObserver observer;
    iVideoEncoder encoder;
    iObject::connect(&encoder, &iVideoEncoder::readyToSubmit, &observer, &VideoResultObserver::onReady);
    iObject::connect(&encoder, &iVideoEncoder::frameRejected, &observer, &VideoResultObserver::onRejected);
    iVideoEncoderSettings settings;
    settings.codec = VideoCodec_H264;
    ASSERT_TRUE(encoder.open(settings, iSize(64, 64)));
    EXPECT_EQ(SubmitResult::InvalidInput, encoder.encodeFrame(iVideoFrame(), 999));
    ASSERT_EQ(1u, observer.rejectionReasons.size());
    EXPECT_EQ(SubmitResult::InvalidInput, observer.rejectionReasons.back());
    const iVideoFrame frame = solidVideoFrame();
    xuint64 accepted = 0;
    while (accepted < 128 && encoder.encodeFrame(frame, accepted) == SubmitResult::Accepted) ++accepted;
    ASSERT_GT(accepted, 0u);
    ASSERT_LT(accepted, 128u);
    ASSERT_EQ(2u, observer.rejectionReasons.size());
    EXPECT_EQ(SubmitResult::WouldBlock, observer.rejectionReasons.back());
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
    while (!observer.ready && std::chrono::steady_clock::now() < deadline)
        iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
    EXPECT_GT(observer.ready, 0);
    EXPECT_EQ(SubmitResult::Accepted, encoder.encode(frame, accepted));
    encoder.close();
}

TEST(GStreamerVideoRegression, SoftwareEncoderPreservesFrameIds)
{
    ASSERT_TRUE(gst_init_check(nullptr, nullptr, nullptr));
    const char* factories[] = {"x264enc", "x265enc"};
    int exercised = 0;
    for (int codec = 0; codec < 2; ++codec) {
        GstElementFactory* factory = gst_element_factory_find(factories[codec]);
        if (!factory) continue;
        gst_object_unref(factory);
        ++exercised;
        SCOPED_TRACE(factories[codec]);
        VideoResultObserver observer;
        iVideoEncoder encoder;
        iObject::connect(&encoder, &iVideoEncoder::packetReady, &observer, &VideoResultObserver::onPacket);
        iVideoEncoderSettings settings;
        settings.codec = static_cast<iVideoCodec>(codec);
        ASSERT_TRUE(encoder.open(settings, iSize(64, 64)));
        iVideoFrame frame(64 * 64 * 3, iSize(64, 64), 64 * 3, iVideoFrame::Format_BGR24);
        ASSERT_TRUE(frame.map(iAbstractVideoBuffer::WriteOnly));
        std::memset(frame.bits(), 96, frame.mappedBytes());
        frame.unmap();
        for (xuint64 index = 0; index < 3; ++index) {
            ASSERT_EQ(SubmitResult::Accepted, encoder.encode(frame, 100 + index));
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(3);
            while (observer.packets.size() <= index && std::chrono::steady_clock::now() < deadline)
                iEventDispatcher::instance()->processEvents(iEventLoop::AllEvents);
            ASSERT_EQ(index + 1, observer.packets.size());
            EXPECT_FALSE(observer.packets.back().data.isEmpty());
            EXPECT_EQ(100 + index, observer.packets.back().frameId);
            EXPECT_TRUE(observer.packets.back().keyFrame);
            EXPECT_GE(observer.packets.back().pts, 0);
            EXPECT_GT(observer.packets.back().duration, 0);
        }
        encoder.close();
    }
    if (!exercised) GTEST_SKIP() << "Software encoders unavailable";
}

TEST(GStreamerVideoRegression, BgrPushPreservesPixelsAndAlignedLayout)
{
    ASSERT_TRUE(gst_init_check(nullptr, nullptr, nullptr));
    const int widths[] = {2, 4};
    for (size_t index = 0; index < 2; ++index) {
        const int width = widths[index];
        for (int padding = 0; padding <= 4; padding += 4) {
            SCOPED_TRACE(width);
            SCOPED_TRACE(padding);
            const int stride = width * 3 + padding;
            iVideoFrame frame(stride * 2, iSize(width, 2), stride, iVideoFrame::Format_BGR24);
            ASSERT_TRUE(frame.map(iAbstractVideoBuffer::WriteOnly));
            std::memset(frame.bits(), 17, stride);
            std::memset(frame.bits() + stride, 29, stride);
            frame.unmap();
            GstElement* pipeline = nullptr;
            GstAppSrc* appsrc = nullptr;
            iString error;
            ASSERT_TRUE(gstBuildPushPipeline(iString::asprintf(
                "appsrc name=src format=time caps=video/x-raw,format=BGR,width=%d,height=2,framerate=30/1 "
                "! appsink name=sink sync=false", width), "src", &pipeline, &appsrc, &error));
            GstElement* sink = gst_bin_get_by_name(GST_BIN(pipeline), "sink");
            EXPECT_EQ(SubmitResult::Accepted, gstPushBgrFrame(appsrc, frame, 0, GST_SECOND / 30));
            gst_app_src_end_of_stream(appsrc);
            GstSample* sample = gst_app_sink_try_pull_sample(GST_APP_SINK(sink), GST_SECOND);
            EXPECT_NE(nullptr, sample);
            if (sample) {
                GstVideoInfo info;
                EXPECT_TRUE(gst_video_info_from_caps(&info, gst_sample_get_caps(sample)));
                GstVideoFrame mapped;
                const bool mappedOk = gst_video_frame_map(&mapped, &info, gst_sample_get_buffer(sample), GST_MAP_READ);
                EXPECT_TRUE(mappedOk);
                if (mappedOk) {
                    const guint8* pixels = static_cast<const guint8*>(GST_VIDEO_FRAME_PLANE_DATA(&mapped, 0));
                    EXPECT_EQ(17, pixels[0]);
                    EXPECT_EQ(29, pixels[GST_VIDEO_FRAME_PLANE_STRIDE(&mapped, 0)]);
                    gst_video_frame_unmap(&mapped);
                }
                gst_sample_unref(sample);
            }
            gst_element_set_state(pipeline, GST_STATE_NULL);
            gst_object_unref(sink);
            gst_object_unref(appsrc);
            gst_object_unref(pipeline);
        }
    }
}

TEST(GStreamerVideoRegression, BgrPushRejectsUndersizedMapping)
{
    ASSERT_TRUE(gst_init_check(nullptr, nullptr, nullptr));
    GstElement* source = gst_element_factory_make("appsrc", nullptr);
    ASSERT_NE(nullptr, source);
    iVideoFrame truncated(6, iSize(2, 2), 6, iVideoFrame::Format_BGR24);
    EXPECT_EQ(SubmitResult::InvalidInput, gstPushBgrFrame(GST_APP_SRC(source), truncated, 0, GST_SECOND / 30));
    iVideoFrame shortStride(8, iSize(2, 2), 4, iVideoFrame::Format_BGR24);
    EXPECT_EQ(SubmitResult::InvalidInput, gstPushBgrFrame(GST_APP_SRC(source), shortStride, 0, GST_SECOND / 30));
    gst_element_set_state(source, GST_STATE_PLAYING);
    gst_element_set_state(source, GST_STATE_NULL);
    EXPECT_EQ(SubmitResult::BackendFailure, gstPushBgrFrame(GST_APP_SRC(source), solidVideoFrame(), 0, GST_SECOND / 30));
    EXPECT_EQ(SubmitResult::Closed, gstPushBgrFrame(nullptr, solidVideoFrame(), 0, GST_SECOND / 30));
    gst_object_unref(source);
}

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

TEST_F(GStreamerRegression, PlayerAcceptsAndDetachesPublicVideoSink)
{
    iGstreamerPlayerSession session(nullptr);
    iGstreamerPlayerControl control(&session);
    iVideoSink* sink = new iVideoSink;
    control.setVideoOutput(sink);
    ASSERT_NE(nullptr, session.renderer());
    EXPECT_TRUE(session.renderer()->isReady());
    delete sink;
    EXPECT_FALSE(session.renderer()->isReady());
    control.setVideoOutput(nullptr);
    EXPECT_EQ(nullptr, session.renderer());
    iCoreApplication::dispatchPostedEvents(nullptr, 0);
}

TEST_F(GStreamerRegression, MissingOutputAdapterUsesPlayerErrorDomain)
{
    iVideoSink sink;
    iMediaPlayer player;
    GstRegistry* registry = gst_registry_get();
    GstPluginFeature* feature = gst_registry_find_feature(registry, "videoconvert", GST_TYPE_ELEMENT_FACTORY);
    ASSERT_NE(nullptr, feature);
    gst_registry_remove_feature(registry, feature);
    player.setVideoOutput(&sink);
    const bool restored = gst_registry_add_feature(registry, feature);
    gst_object_unref(feature);
    EXPECT_TRUE(restored);
    EXPECT_EQ(iMediaPlayer::ResourceError, player.error());
    EXPECT_TRUE(player.errorString().contains(iString("video sink adapter")));
}

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