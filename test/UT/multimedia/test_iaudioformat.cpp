/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    test_iaudioformat.cpp
/// @brief   Unit tests for iAudioFormat class
/// @version 1.0
/// @author  Unit Test Generator
/////////////////////////////////////////////////////////////////

#include <gtest/gtest.h>
#include <multimedia/audio/iaudioformat.h>

using namespace iShell;

// ============================================================================
// Test Fixture
// ============================================================================

class IAudioFormatTest : public ::testing::Test {
protected:
    // A fully specified, valid PCM format: 44.1 kHz, 16-bit, stereo.
    static iAudioFormat validFormat() {
        iAudioFormat fmt;
        fmt.setSampleRate(44100);
        fmt.setChannelCount(2);
        fmt.setSampleSize(16);
        fmt.setCodec("audio/pcm");
        fmt.setSampleType(iAudioFormat::SignedInt);
        fmt.setByteOrder(iAudioFormat::LittleEndian);
        return fmt;
    }
};

// ============================================================================
// Construction / defaults
// ============================================================================

TEST_F(IAudioFormatTest, DefaultConstructionIsInvalid) {
    iAudioFormat fmt;
    EXPECT_EQ(fmt.sampleRate(), -1);
    EXPECT_EQ(fmt.channelCount(), -1);
    EXPECT_EQ(fmt.sampleSize(), -1);
    EXPECT_EQ(fmt.sampleType(), iAudioFormat::Unknown);
    EXPECT_TRUE(fmt.codec().isEmpty());
    EXPECT_FALSE(fmt.isValid());
}

TEST_F(IAudioFormatTest, SettersAndGettersRoundTrip) {
    iAudioFormat fmt;
    fmt.setSampleRate(48000);
    fmt.setChannelCount(1);
    fmt.setSampleSize(24);
    fmt.setCodec("audio/pcm");
    fmt.setSampleType(iAudioFormat::Float);
    fmt.setByteOrder(iAudioFormat::BigEndian);

    EXPECT_EQ(fmt.sampleRate(), 48000);
    EXPECT_EQ(fmt.channelCount(), 1);
    EXPECT_EQ(fmt.sampleSize(), 24);
    EXPECT_EQ(fmt.codec(), iString("audio/pcm"));
    EXPECT_EQ(fmt.sampleType(), iAudioFormat::Float);
    EXPECT_EQ(fmt.byteOrder(), iAudioFormat::BigEndian);
}

// ============================================================================
// Validity
// ============================================================================

TEST_F(IAudioFormatTest, ValidFormatIsValid) {
    EXPECT_TRUE(validFormat().isValid());
}

TEST_F(IAudioFormatTest, MissingCodecIsInvalid) {
    iAudioFormat fmt = validFormat();
    fmt.setCodec(iString());
    EXPECT_FALSE(fmt.isValid());
}

TEST_F(IAudioFormatTest, UnknownSampleTypeIsInvalid) {
    iAudioFormat fmt = validFormat();
    fmt.setSampleType(iAudioFormat::Unknown);
    EXPECT_FALSE(fmt.isValid());
}

// ============================================================================
// Frame / byte / duration conversions
// ============================================================================

TEST_F(IAudioFormatTest, BytesPerFrame) {
    // 16-bit * 2 channels / 8 = 4 bytes per frame.
    EXPECT_EQ(validFormat().bytesPerFrame(), 4);
    // Invalid format has zero bytes per frame.
    EXPECT_EQ(iAudioFormat().bytesPerFrame(), 0);
}

TEST_F(IAudioFormatTest, BytesForFrames) {
    EXPECT_EQ(validFormat().bytesForFrames(100), 400);
    EXPECT_EQ(iAudioFormat().bytesForFrames(100), 0);
}

TEST_F(IAudioFormatTest, FramesForBytes) {
    EXPECT_EQ(validFormat().framesForBytes(400), 100);
    // Rounds down to whole frames.
    EXPECT_EQ(validFormat().framesForBytes(401), 100);
    EXPECT_EQ(iAudioFormat().framesForBytes(400), 0);
}

TEST_F(IAudioFormatTest, FramesForDuration) {
    // 1 second at 44100 Hz = 44100 frames.
    EXPECT_EQ(validFormat().framesForDuration(1000000LL), 44100);
    EXPECT_EQ(iAudioFormat().framesForDuration(1000000LL), 0);
}

TEST_F(IAudioFormatTest, DurationForFrames) {
    EXPECT_EQ(validFormat().durationForFrames(44100), 1000000LL);
    EXPECT_EQ(validFormat().durationForFrames(0), 0);
    EXPECT_EQ(iAudioFormat().durationForFrames(44100), 0);
}

TEST_F(IAudioFormatTest, BytesForDuration) {
    // bytesPerFrame(4) * framesForDuration(44100) = 176400.
    EXPECT_EQ(validFormat().bytesForDuration(1000000LL), 176400);
}

TEST_F(IAudioFormatTest, DurationForBytes) {
    EXPECT_EQ(validFormat().durationForBytes(176400), 1000000LL);
    EXPECT_EQ(validFormat().durationForBytes(0), 0);
    EXPECT_EQ(iAudioFormat().durationForBytes(176400), 0);
}

// ============================================================================
// Equality / copy semantics
// ============================================================================

TEST_F(IAudioFormatTest, EqualityOperators) {
    iAudioFormat a = validFormat();
    iAudioFormat b = validFormat();
    EXPECT_TRUE(a == b);
    EXPECT_FALSE(a != b);

    b.setSampleRate(22050);
    EXPECT_FALSE(a == b);
    EXPECT_TRUE(a != b);
}

TEST_F(IAudioFormatTest, CopyConstructorPreservesValues) {
    iAudioFormat a = validFormat();
    iAudioFormat b(a);
    EXPECT_TRUE(a == b);
}

TEST_F(IAudioFormatTest, AssignmentPreservesValues) {
    iAudioFormat a = validFormat();
    iAudioFormat b;
    b = a;
    EXPECT_TRUE(a == b);
}
