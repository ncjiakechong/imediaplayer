/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    iaudiobuffer.h
/// @brief   designed to hold audio data, providing methods for 
///          accessing, modifying, and querying the audio content
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IAUDIOBUFFER_H
#define IAUDIOBUFFER_H

#include <core/kernel/iobject.h>
#include <core/utils/ishareddata.h>

#include <multimedia/imultimediaglobal.h>
#include <multimedia/audio/iaudioformat.h>

namespace iShell {

class iAbstractAudioBuffer;
class iAudioBufferPrivate;
class IX_MULTIMEDIA_EXPORT iAudioBuffer
{
public:
    iAudioBuffer();
    iAudioBuffer(iAbstractAudioBuffer *provider);
    iAudioBuffer(const iAudioBuffer &other);
    iAudioBuffer(const iByteArray &data, const iAudioFormat &format, xint64 startTime = -1);
    iAudioBuffer(int numFrames, const iAudioFormat &format, xint64 startTime = -1); // Initialized to empty

    iAudioBuffer& operator=(const iAudioBuffer &other);

    ~iAudioBuffer();

    bool isValid() const;

    iAudioFormat format() const;

    int frameCount() const;
    int sampleCount() const;
    int byteCount() const;

    xint64 duration() const;
    xint64 startTime() const;

    // Data modification
    // void clear();
    // Other ideas
    // operator *=
    // operator += (need to be careful about different formats)

    // Data access
    const void* constData() const; // Does not detach, preferred
    const void* data() const; // Does not detach
    void *data(); // detaches

    // Structures for easier access to stereo data
    template <typename T> struct StereoFrameDefault { enum { Default = 0 }; };

    template <typename T> struct StereoFrame {

        StereoFrame()
            : left(T(StereoFrameDefault<T>::Default))
            , right(T(StereoFrameDefault<T>::Default))
        {}

        StereoFrame(T leftSample, T rightSample)
            : left(leftSample)
            , right(rightSample)
        {}

        StereoFrame& operator=(const StereoFrame &other)
        {
            // Two straight assigns is probably
            // cheaper than a conditional check on
            // self assignment
            left = other.left;
            right = other.right;
            return *this;
        }

        T left;
        T right;

        T average() const {return (left + right) / 2;}
        void clear() {left = right = T(StereoFrameDefault<T>::Default);}
    };

    typedef StereoFrame<unsigned char> S8U;
    typedef StereoFrame<signed char> S8S;
    typedef StereoFrame<unsigned short> S16U;
    typedef StereoFrame<signed short> S16S;
    typedef StereoFrame<float> S32F;

    template <typename T> const T* constData() const {
        return static_cast<const T*>(constData());
    }
    template <typename T> const T* data() const {
        return static_cast<const T*>(data());
    }
    template <typename T> T* data() {
        return static_cast<T*>(data());
    }
private:
    iAudioBufferPrivate *d;
};

template <> struct iAudioBuffer::StereoFrameDefault<unsigned char> { enum { Default = 128 }; };
template <> struct iAudioBuffer::StereoFrameDefault<unsigned short> { enum { Default = 32768 }; };

} // namespace iShell
#endif // IAUDIOBUFFER_H
