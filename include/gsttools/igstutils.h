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
#ifndef IGSTUTILS_H
#define IGSTUTILS_H

#include <map>

#include <gst/gst.h>
#include <gst/video/video.h>

#if GST_CHECK_VERSION(1,0,0)
# define QT_GSTREAMER_PLAYBIN_ELEMENT_NAME "playbin"
# define QT_GSTREAMER_CAMERABIN_ELEMENT_NAME "camerabin"
# define QT_GSTREAMER_COLORCONVERSION_ELEMENT_NAME "videoconvert"
# define QT_GSTREAMER_RAW_AUDIO_MIME "audio/x-raw"
# define QT_GSTREAMER_VIDEOOVERLAY_INTERFACE_NAME "GstVideoOverlay"
#else
# define QT_GSTREAMER_PLAYBIN_ELEMENT_NAME "playbin2"
# define QT_GSTREAMER_CAMERABIN_ELEMENT_NAME "camerabin2"
# define QT_GSTREAMER_COLORCONVERSION_ELEMENT_NAME "ffmpegcolorspace"
# define QT_GSTREAMER_RAW_AUDIO_MIME "audio/x-raw-int"
# define QT_GSTREAMER_VIDEOOVERLAY_INTERFACE_NAME "GstXOverlay"
#endif

namespace ishell {

class iSize;
class iVariant;
class QVideoSurfaceFormat;

namespace iGstUtils {

    std::map<std::string, iVariant> gstTagListToMap(const GstTagList *list);

#if 0
    iSize capsResolution(const GstCaps *caps);
    iSize capsCorrectedResolution(const GstCaps *caps);
    QAudioFormat audioFormatForCaps(const GstCaps *caps);
#if GST_CHECK_VERSION(1,0,0)
    QAudioFormat audioFormatForSample(GstSample *sample);
#else
    QAudioFormat audioFormatForBuffer(GstBuffer *buffer);
#endif
    GstCaps *capsForAudioFormat(const QAudioFormat &format);
    void initializeGst();
    QMultimedia::SupportEstimate hasSupport(const QString &mimeType,
                                             const QStringList &codecs,
                                             const QSet<QString> &supportedMimeTypeSet);

    QSet<QString> supportedMimeTypes(bool (*isValidFactory)(GstElementFactory *factory));

#if GST_CHECK_VERSION(1,0,0)
    QImage bufferToImage(GstBuffer *buffer, const GstVideoInfo &info);
    QVideoSurfaceFormat formatForCaps(
            GstCaps *caps,
            GstVideoInfo *info = 0,
            QAbstractVideoBuffer::HandleType handleType = QAbstractVideoBuffer::NoHandle);
#else
    QImage bufferToImage(GstBuffer *buffer);
    QVideoSurfaceFormat formatForCaps(
            GstCaps *caps,
            int *bytesPerLine = 0,
            QAbstractVideoBuffer::HandleType handleType = QAbstractVideoBuffer::NoHandle);
#endif

    GstCaps *capsForFormats(const QList<QVideoFrame::PixelFormat> &formats);
    void setFrameTimeStamps(QVideoFrame *frame, GstBuffer *buffer);

    void setMetaData(GstElement *element, const QMap<QByteArray, iVariant> &data);
    void setMetaData(GstBin *bin, const QMap<QByteArray, iVariant> &data);

    GstCaps *videoFilterCaps();

    iSize structureResolution(const GstStructure *s);
    QVideoFrame::PixelFormat structurePixelFormat(const GstStructure *s, int *bpp = 0);
    iSize structurePixelAspectRatio(const GstStructure *s);
    QPair<qreal, qreal> structureFrameRateRange(const GstStructure *s);

    QString fileExtensionForMimeType(const QString &mimeType);

    void qt_gst_object_ref_sink(gpointer object);
    GstCaps *qt_gst_pad_get_current_caps(GstPad *pad);
    GstCaps *qt_gst_pad_get_caps(GstPad *pad);
    GstStructure *qt_gst_structure_new_empty(const char *name);
    gboolean qt_gst_element_query_position(GstElement *element, GstFormat format, gint64 *cur);
    gboolean qt_gst_element_query_duration(GstElement *element, GstFormat format, gint64 *cur);
    GstCaps *qt_gst_caps_normalize(GstCaps *caps);
    const gchar *qt_gst_element_get_factory_name(GstElement *element);
    gboolean qt_gst_caps_can_intersect(const GstCaps * caps1, const GstCaps * caps2);
    GList *qt_gst_video_sinks();
    void qt_gst_util_double_to_fraction(gdouble src, gint *dest_n, gint *dest_d);

#endif
}


} // namespace ishell

#endif // IGSTUTILS_H
