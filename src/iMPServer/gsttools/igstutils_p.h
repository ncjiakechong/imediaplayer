/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstutils.h
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////
#ifndef IGSTUTILS_P_H
#define IGSTUTILS_P_H

//
//  W A R N I N G
//  -------------
//
// This file is not part of the Qt API. It exists purely as an
// implementation detail. This header file may change from version to
// version without notice, or even be removed.
//
// We mean it.
//

#include <map>
#include <unordered_set>
#include <unordered_map>

#include <gst/gst.h>
#include <gst/video/video.h>

#include <core/kernel/ivariant.h>
#include <core/utils/istring.h>
#include <core/utils/ihashfunctions.h>

#include <multimedia/imultimedia.h>
#include <multimedia/audio/iaudioformat.h>
#include <multimedia/video/ivideoframe.h>

#if GST_CHECK_VERSION(1,0,0)
# define IX_GSTREAMER_PLAYBIN_ELEMENT_NAME "playbin"
# define IX_GSTREAMER_CAMERABIN_ELEMENT_NAME "camerabin"
# define IX_GSTREAMER_COLORCONVERSION_ELEMENT_NAME "videoconvert"
# define IX_GSTREAMER_RAW_AUDIO_MIME "audio/x-raw"
# define IX_GSTREAMER_VIDEOOVERLAY_INTERFACE_NAME "GstVideoOverlay"
#else
# define IX_GSTREAMER_PLAYBIN_ELEMENT_NAME "playbin2"
# define IX_GSTREAMER_CAMERABIN_ELEMENT_NAME "camerabin2"
# define IX_GSTREAMER_COLORCONVERSION_ELEMENT_NAME "ffmpegcolorspace"
# define IX_GSTREAMER_RAW_AUDIO_MIME "audio/x-raw-int"
# define IX_GSTREAMER_VIDEOOVERLAY_INTERFACE_NAME "GstXOverlay"
#endif

namespace iShell {

class iSize;
class iVariant;
class iByteArray;
class iVideoSurfaceFormat;

namespace iGstUtils {

    std::multimap<iByteArray, iVariant> gstTagListToMap(const GstTagList *list);

    iSize capsResolution(const GstCaps *caps);
    iSize capsCorrectedResolution(const GstCaps *caps);
    iAudioFormat audioFormatForCaps(const GstCaps *caps);
    #if GST_CHECK_VERSION(1,0,0)
    iAudioFormat audioFormatForSample(GstSample *sample);
    #else
    iAudioFormat audioFormatForBuffer(GstBuffer *buffer);
    #endif
    GstCaps *capsForAudioFormat(const iAudioFormat &format);
    void initializeGst();
    iMultimedia::SupportEstimate hasSupport(const iString &mimeType,
                                             const std::list<iString> &codecs,
                                             const std::unordered_set<iString, iKeyHashFunc> &supportedMimeTypeSet);


    std::unordered_set<iString, iKeyHashFunc> supportedMimeTypes(bool (*isValidFactory)(GstElementFactory *factory));

    #if GST_CHECK_VERSION(1,0,0)
    iVideoSurfaceFormat formatForCaps(
            GstCaps *caps,
            GstVideoInfo *info = IX_NULLPTR,
            iAbstractVideoBuffer::HandleType handleType = iAbstractVideoBuffer::NoHandle);
    #else
    iVideoSurfaceFormat formatForCaps(
            GstCaps *caps,
            int *bytesPerLine = 0,
            iAbstractVideoBuffer::HandleType handleType = iAbstractVideoBuffer::NoHandle);
    #endif

    GstCaps *capsForFormats(const std::list<iVideoFrame::PixelFormat> &formats);
    void setFrameTimeStamps(iVideoFrame *frame, GstBuffer *buffer);

    void setMetaData(GstElement *element, const std::multimap<iByteArray, iVariant> &data);
    void setMetaData(GstBin *bin, const std::multimap<iByteArray, iVariant> &data);

    GstCaps *videoFilterCaps();

    iSize structureResolution(const GstStructure *s);
    iVideoFrame::PixelFormat structurePixelFormat(const GstStructure *s, int *bpp = 0);
    iSize structurePixelAspectRatio(const GstStructure *s);
    std::pair<xreal, xreal> structureFrameRateRange(const GstStructure *s);

    iString fileExtensionForMimeType(const iString &mimeType);

    #if GST_CHECK_VERSION(0,10,30)
    iVariant fromGStreamerOrientation(const iVariant &value);
    iVariant toGStreamerOrientation(const iVariant &value);
    #endif
}

void ix_gst_object_ref_sink(gpointer object);
GstCaps *ix_gst_pad_get_current_caps(GstPad *pad);
GstCaps *ix_gst_pad_get_caps(GstPad *pad);
GstStructure *ix_gst_structure_new_empty(const char *name);
gboolean ix_gst_element_query_position(GstElement *element, GstFormat format, gint64 *cur);
gboolean ix_gst_element_query_duration(GstElement *element, GstFormat format, gint64 *cur);
GstCaps *ix_gst_caps_normalize(GstCaps *caps);
const gchar *ix_gst_element_get_factory_name(GstElement *element);
gboolean ix_gst_caps_can_intersect(const GstCaps * caps1, const GstCaps * caps2);
GList *ix_gst_video_sinks();
void ix_gst_util_double_to_fraction(gdouble src, gint *dest_n, gint *dest_d);

} // namespace iShell

#endif
