#include <core/kernel/iobject.h>
#include <core/kernel/icoreapplication.h>
#ifdef IX_TEST_MULTIMEDIA
#include <multimedia/codec/ivideodecoder.h>
#include <multimedia/codec/ivideoencoder.h>
#include <multimedia/video/ivideosink.h>
#include <multimedia/recording/imediarecorder.h>
#include <multimedia/playback/imediaplayer.h>
#endif

class ExternalObject : public iShell::iObject {};

int main(int argc, char** argv)
{
    iShell::iCoreApplication application(argc, argv);
    ExternalObject object;
    if (!object.metaObject()) return 1;
#ifdef IX_TEST_MULTIMEDIA
    iShell::iVideoAccessUnit unit;
    if (unit.pts != iShell::iVideoAccessUnit::InvalidTime) return 2;
    iShell::iVideoEncoder encoder;
    iShell::iVideoDecoder decoder;
    iShell::iVideoSink sink;
    iShell::iMediaRecorder recorder;
    if (!iShell::iObject::connect(&encoder, &iShell::iVideoEncoder::packetReady,
            &decoder, &iShell::iVideoDecoder::submitAccessUnit)) return 3;
    if (!iShell::iObject::connect(&decoder, &iShell::iVideoDecoder::frameReady,
            &sink, &iShell::iVideoSink::setVideoFrame)) return 4;
    if (!recorder.finalize(0) || recorder.framesAccepted() != 0) return 5;
    if (!sink.open(iShell::iString::fromUtf8("fakesink", -1), iShell::iSize(4, 4))) return 6;
    sink.close();
    if (encoder.isDraining() || decoder.isDraining() || recorder.isFinalizing()) return 7;
    if (encoder.endOfStream() || decoder.endOfStream() || !recorder.stop(0)) return 8;
        if (encoder.encodeFrame(iShell::iVideoFrame(), 1) != iShell::iMultimedia::SubmitResult::Closed
            || recorder.writeFrame(iShell::iVideoFrame(), 2) != iShell::iMultimedia::SubmitResult::Closed) return 9;
    iShell::iVideoEncoderSettings settings;
    if (settings.timestampPolicy != iShell::VideoTimestamp_Preserve) return 10;
    sink.setVideoFrame(iShell::iVideoFrame());
    if (sink.videoFrame().isValid()) return 11;
    iShell::iMediaPlayer player;
    player.setVideoOutput(&sink);
    player.setVideoOutput(nullptr);
#endif
    return 0;
}