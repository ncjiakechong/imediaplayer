/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstutils.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////

#include <linux/videodev2.h>
#include <gst/audio/audio.h>
#include <gst/video/video.h>

#include "core/kernel/ivariant.h"
#include "gsttools/igstutils.h"

namespace iShell {

template<typename T, int N> static int lengthOf(const T (&)[N]) { return N; }

//internal
static void addTagToMap(const GstTagList *list,
                        const gchar *tag,
                        gpointer user_data)
{
    std::map<std::string, iVariant> *map = reinterpret_cast<std::map<std::string, iVariant>* >(user_data);

    GValue val;
    val.g_type = 0;
    gst_tag_list_copy_value(&val,list,tag);

    switch( G_VALUE_TYPE(&val) ) {
        case G_TYPE_STRING:
        {
            const gchar *str_value = g_value_get_string(&val);
            map->insert(std::pair<std::string, iVariant >(std::string(tag), std::string(str_value)));
            break;
        }
        case G_TYPE_INT:
            map->insert(std::pair<std::string, iVariant >(std::string(tag), g_value_get_int(&val)));
            break;
        case G_TYPE_UINT:
            map->insert(std::pair<std::string, iVariant >(std::string(tag), g_value_get_uint(&val)));
            break;
        case G_TYPE_LONG:
            map->insert(std::pair<std::string, iVariant >(std::string(tag), xint64(g_value_get_long(&val))));
            break;
        case G_TYPE_BOOLEAN:
            map->insert(std::pair<std::string, iVariant >(std::string(tag), g_value_get_boolean(&val)));
            break;
        case G_TYPE_CHAR:
#if GLIB_CHECK_VERSION(2,32,0)
            map->insert(std::pair<std::string, iVariant >(std::string(tag), g_value_get_schar(&val)));
#else
            map->insert(std::pair<std::string, iVariant >(std::string(tag), g_value_get_char(&val)));
#endif
            break;
        case G_TYPE_DOUBLE:
            map->insert(std::pair<std::string, iVariant >(std::string(tag), g_value_get_double(&val)));
            break;
        default:
            // GST_TYPE_DATE is a function, not a constant, so pull it out of the switch
            break;
    }

    g_value_unset(&val);
}

/*!
  Convert GstTagList structure to std::map<std::string, iVariant>.

  Mapping to int, bool, char, string, fractions and date are supported.
  Fraction values are converted to doubles.
*/
std::map<std::string, iVariant> iGstUtils::gstTagListToMap(const GstTagList *tags)
{
    std::map<std::string, iVariant> res;
    gst_tag_list_foreach(tags, addTagToMap, &res);

    return res;
}

#if 0
/*!
  Returns resolution of \a caps.
  If caps doesn't have a valid size, and ampty iSize is returned.
*/
iSize iGstUtils::capsResolution(const GstCaps *caps)
{
    if (gst_caps_get_size(caps) == 0)
        return iSize();

    return structureResolution(gst_caps_get_structure(caps, 0));
}

/*!
  Returns aspect ratio corrected resolution of \a caps.
  If caps doesn't have a valid size, an empty iSize is returned.
*/
iSize iGstUtils::capsCorrectedResolution(const GstCaps *caps)
{
    iSize size;

    if (caps) {
        size = capsResolution(caps);

        gint aspectNum = 0;
        gint aspectDenum = 0;
        if (!size.isEmpty() && gst_structure_get_fraction(
                    gst_caps_get_structure(caps, 0), "pixel-aspect-ratio", &aspectNum, &aspectDenum)) {
            if (aspectDenum > 0)
                size.setWidth(size.width()*aspectNum/aspectDenum);
        }
    }

    return size;
}


#if GST_CHECK_VERSION(1,0,0)
namespace {

struct AudioFormat
{
    GstAudioFormat format;
    QAudioFormat::SampleType sampleType;
    QAudioFormat::Endian byteOrder;
    int sampleSize;
};
static const AudioFormat qt_audioLookup[] =
{
    { GST_AUDIO_FORMAT_S8   , QAudioFormat::SignedInt  , QAudioFormat::LittleEndian, 8  },
    { GST_AUDIO_FORMAT_U8   , QAudioFormat::UnSignedInt, QAudioFormat::LittleEndian, 8  },
    { GST_AUDIO_FORMAT_S16LE, QAudioFormat::SignedInt  , QAudioFormat::LittleEndian, 16 },
    { GST_AUDIO_FORMAT_S16BE, QAudioFormat::SignedInt  , QAudioFormat::BigEndian   , 16 },
    { GST_AUDIO_FORMAT_U16LE, QAudioFormat::UnSignedInt, QAudioFormat::LittleEndian, 16 },
    { GST_AUDIO_FORMAT_U16BE, QAudioFormat::UnSignedInt, QAudioFormat::BigEndian   , 16 },
    { GST_AUDIO_FORMAT_S32LE, QAudioFormat::SignedInt  , QAudioFormat::LittleEndian, 32 },
    { GST_AUDIO_FORMAT_S32BE, QAudioFormat::SignedInt  , QAudioFormat::BigEndian   , 32 },
    { GST_AUDIO_FORMAT_U32LE, QAudioFormat::UnSignedInt, QAudioFormat::LittleEndian, 32 },
    { GST_AUDIO_FORMAT_U32BE, QAudioFormat::UnSignedInt, QAudioFormat::BigEndian   , 32 },
    { GST_AUDIO_FORMAT_S24LE, QAudioFormat::SignedInt  , QAudioFormat::LittleEndian, 24 },
    { GST_AUDIO_FORMAT_S24BE, QAudioFormat::SignedInt  , QAudioFormat::BigEndian   , 24 },
    { GST_AUDIO_FORMAT_U24LE, QAudioFormat::UnSignedInt, QAudioFormat::LittleEndian, 24 },
    { GST_AUDIO_FORMAT_U24BE, QAudioFormat::UnSignedInt, QAudioFormat::BigEndian   , 24 },
    { GST_AUDIO_FORMAT_F32LE, QAudioFormat::Float      , QAudioFormat::LittleEndian, 32 },
    { GST_AUDIO_FORMAT_F32BE, QAudioFormat::Float      , QAudioFormat::BigEndian   , 32 },
    { GST_AUDIO_FORMAT_F64LE, QAudioFormat::Float      , QAudioFormat::LittleEndian, 64 },
    { GST_AUDIO_FORMAT_F64BE, QAudioFormat::Float      , QAudioFormat::BigEndian   , 64 }
};

}
#endif

/*!
  Returns audio format for caps.
  If caps doesn't have a valid audio format, an empty QAudioFormat is returned.
*/

QAudioFormat iGstUtils::audioFormatForCaps(const GstCaps *caps)
{
    QAudioFormat format;
#if GST_CHECK_VERSION(1,0,0)
    GstAudioInfo info;
    if (gst_audio_info_from_caps(&info, caps)) {
        for (int i = 0; i < lengthOf(qt_audioLookup); ++i) {
            if (qt_audioLookup[i].format != info.finfo->format)
                continue;

            format.setSampleType(qt_audioLookup[i].sampleType);
            format.setByteOrder(qt_audioLookup[i].byteOrder);
            format.setSampleSize(qt_audioLookup[i].sampleSize);
            format.setSampleRate(info.rate);
            format.setChannelCount(info.channels);
            format.setCodec(QStringLiteral("audio/pcm"));

            return format;
        }
    }
#else
    const GstStructure *structure = gst_caps_get_structure(caps, 0);

    if (istrcmp(gst_structure_get_name(structure), "audio/x-raw-int") == 0) {

        format.setCodec("audio/pcm");

        int endianness = 0;
        gst_structure_get_int(structure, "endianness", &endianness);
        if (endianness == 1234)
            format.setByteOrder(QAudioFormat::LittleEndian);
        else if (endianness == 4321)
            format.setByteOrder(QAudioFormat::BigEndian);

        gboolean isSigned = FALSE;
        gst_structure_get_boolean(structure, "signed", &isSigned);
        if (isSigned)
            format.setSampleType(QAudioFormat::SignedInt);
        else
            format.setSampleType(QAudioFormat::UnSignedInt);

        // Number of bits allocated per sample.
        int width = 0;
        gst_structure_get_int(structure, "width", &width);

        // The number of bits used per sample. This must be less than or equal to the width.
        int depth = 0;
        gst_structure_get_int(structure, "depth", &depth);

        if (width != depth) {
            // Unsupported sample layout.
            return QAudioFormat();
        }
        format.setSampleSize(width);

        int rate = 0;
        gst_structure_get_int(structure, "rate", &rate);
        format.setSampleRate(rate);

        int channels = 0;
        gst_structure_get_int(structure, "channels", &channels);
        format.setChannelCount(channels);

    } else if (istrcmp(gst_structure_get_name(structure), "audio/x-raw-float") == 0) {

        format.setCodec("audio/pcm");

        int endianness = 0;
        gst_structure_get_int(structure, "endianness", &endianness);
        if (endianness == 1234)
            format.setByteOrder(QAudioFormat::LittleEndian);
        else if (endianness == 4321)
            format.setByteOrder(QAudioFormat::BigEndian);

        format.setSampleType(QAudioFormat::Float);

        int width = 0;
        gst_structure_get_int(structure, "width", &width);

        format.setSampleSize(width);

        int rate = 0;
        gst_structure_get_int(structure, "rate", &rate);
        format.setSampleRate(rate);

        int channels = 0;
        gst_structure_get_int(structure, "channels", &channels);
        format.setChannelCount(channels);

    } else {
        return QAudioFormat();
    }
#endif
    return format;
}

#if GST_CHECK_VERSION(1,0,0)
/*!
  Returns audio format for a sample.
  If the buffer doesn't have a valid audio format, an empty QAudioFormat is returned.
*/
QAudioFormat iGstUtils::audioFormatForSample(GstSample *sample)
{
    GstCaps* caps = gst_sample_get_caps(sample);
    if (!caps)
        return QAudioFormat();

    return iGstUtils::audioFormatForCaps(caps);
}
#else
/*!
  Returns audio format for a buffer.
  If the buffer doesn't have a valid audio format, an empty QAudioFormat is returned.
*/
QAudioFormat iGstUtils::audioFormatForBuffer(GstBuffer *buffer)
{
    GstCaps* caps = gst_buffer_get_caps(buffer);
    if (!caps)
        return QAudioFormat();

    QAudioFormat format = iGstUtils::audioFormatForCaps(caps);
    gst_caps_unref(caps);
    return format;
}
#endif

/*!
  Builds GstCaps for an audio format.
  Returns 0 if the audio format is not valid.
  Caller must unref GstCaps.
*/

GstCaps *iGstUtils::capsForAudioFormat(const QAudioFormat &format)
{
    if (!format.isValid())
        return 0;

#if GST_CHECK_VERSION(1,0,0)
    const QAudioFormat::SampleType sampleType = format.sampleType();
    const QAudioFormat::Endian byteOrder = format.byteOrder();
    const int sampleSize = format.sampleSize();

    for (int i = 0; i < lengthOf(qt_audioLookup); ++i) {
        if (qt_audioLookup[i].sampleType != sampleType
                || qt_audioLookup[i].byteOrder != byteOrder
                || qt_audioLookup[i].sampleSize != sampleSize) {
            continue;
        }

        return gst_caps_new_simple(
                    "audio/x-raw",
                    "format"  , G_TYPE_STRING, gst_audio_format_to_string(qt_audioLookup[i].format),
                    "rate"    , G_TYPE_INT   , format.sampleRate(),
                    "channels", G_TYPE_INT   , format.channelCount(),
                    NULL);
    }
    return 0;
#else
    GstStructure *structure = 0;

    if (format.isValid()) {
        if (format.sampleType() == QAudioFormat::SignedInt || format.sampleType() == QAudioFormat::UnSignedInt) {
            structure = gst_structure_new("audio/x-raw-int", NULL);
        } else if (format.sampleType() == QAudioFormat::Float) {
            structure = gst_structure_new("audio/x-raw-float", NULL);
        }
    }

    GstCaps *caps = 0;

    if (structure) {
        gst_structure_set(structure, "rate", G_TYPE_INT, format.sampleRate(), NULL);
        gst_structure_set(structure, "channels", G_TYPE_INT, format.channelCount(), NULL);
        gst_structure_set(structure, "width", G_TYPE_INT, format.sampleSize(), NULL);
        gst_structure_set(structure, "depth", G_TYPE_INT, format.sampleSize(), NULL);

        if (format.byteOrder() == QAudioFormat::LittleEndian)
            gst_structure_set(structure, "endianness", G_TYPE_INT, 1234, NULL);
        else if (format.byteOrder() == QAudioFormat::BigEndian)
            gst_structure_set(structure, "endianness", G_TYPE_INT, 4321, NULL);

        if (format.sampleType() == QAudioFormat::SignedInt)
            gst_structure_set(structure, "signed", G_TYPE_BOOLEAN, TRUE, NULL);
        else if (format.sampleType() == QAudioFormat::UnSignedInt)
            gst_structure_set(structure, "signed", G_TYPE_BOOLEAN, FALSE, NULL);

        caps = gst_caps_new_empty();
        ix_assert(caps);
        gst_caps_append_structure(caps, structure);
    }

    return caps;
#endif
}

void iGstUtils::initializeGst()
{
    static bool initialized = false;
    if (!initialized) {
        initialized = true;
        gst_init(NULL, NULL);
    }
}

namespace {
    const char* getCodecAlias(const std::string &codec)
    {
        if (codec.startsWith("avc1."))
            return "video/x-h264";

        if (codec.startsWith("mp4a."))
            return "audio/mpeg4";

        if (codec.startsWith("mp4v.20."))
            return "video/mpeg4";

        if (codec == "samr")
            return "audio/amr";

        return 0;
    }

    const char* getMimeTypeAlias(const iString &mimeType)
    {
        if (mimeType == "video/mp4")
            return "video/mpeg4";

        if (mimeType == "audio/mp4")
            return "audio/mpeg4";

        if (mimeType == "video/ogg"
            || mimeType == "audio/ogg")
            return "application/ogg";

        return 0;
    }
}

QMultimedia::SupportEstimate iGstUtils::hasSupport(const iString &mimeType,
                                                    const QStringList &codecs,
                                                    const QSet<iString> &supportedMimeTypeSet)
{
    if (supportedMimeTypeSet.isEmpty())
        return QMultimedia::NotSupported;

    iString mimeTypeLowcase = mimeType.toLower();
    bool containsMimeType = supportedMimeTypeSet.contains(mimeTypeLowcase);
    if (!containsMimeType) {
        const char* mimeTypeAlias = getMimeTypeAlias(mimeTypeLowcase);
        containsMimeType = supportedMimeTypeSet.contains(mimeTypeAlias);
        if (!containsMimeType) {
            containsMimeType = supportedMimeTypeSet.contains("video/" + mimeTypeLowcase)
                               || supportedMimeTypeSet.contains("video/x-" + mimeTypeLowcase)
                               || supportedMimeTypeSet.contains("audio/" + mimeTypeLowcase)
                               || supportedMimeTypeSet.contains("audio/x-" + mimeTypeLowcase);
        }
    }

    int supportedCodecCount = 0;
    for (const iString &codec : codecs) {
        iString codecLowcase = codec.toLower();
        const char* codecAlias = getCodecAlias(codecLowcase);
        if (codecAlias) {
            if (supportedMimeTypeSet.contains(codecAlias))
                supportedCodecCount++;
        } else if (supportedMimeTypeSet.contains("video/" + codecLowcase)
                   || supportedMimeTypeSet.contains("video/x-" + codecLowcase)
                   || supportedMimeTypeSet.contains("audio/" + codecLowcase)
                   || supportedMimeTypeSet.contains("audio/x-" + codecLowcase)) {
            supportedCodecCount++;
        }
    }
    if (supportedCodecCount > 0 && supportedCodecCount == codecs.size())
        return QMultimedia::ProbablySupported;

    if (supportedCodecCount == 0 && !containsMimeType)
        return QMultimedia::NotSupported;

    return QMultimedia::MaybeSupported;
}


QSet<iString> iGstUtils::supportedMimeTypes(bool (*isValidFactory)(GstElementFactory *factory))
{
    QSet<iString> supportedMimeTypes;

    //enumerate supported mime types
    gst_init(NULL, NULL);

#if GST_CHECK_VERSION(1,0,0)
    GstRegistry *registry = gst_registry_get();
    GList *orig_plugins = gst_registry_get_plugin_list(registry);
#else
    GstRegistry *registry = gst_registry_get_default();
    GList *orig_plugins = gst_default_registry_get_plugin_list ();
#endif
    for (GList *plugins = orig_plugins; plugins; plugins = g_list_next(plugins)) {
        GstPlugin *plugin = (GstPlugin *) (plugins->data);
#if GST_CHECK_VERSION(1,0,0)
        if (GST_OBJECT_FLAG_IS_SET(GST_OBJECT(plugin), GST_PLUGIN_FLAG_BLACKLISTED))
            continue;
#else
        if (plugin->flags & (1<<1)) //GST_PLUGIN_FLAG_BLACKLISTED
            continue;
#endif

        GList *orig_features = gst_registry_get_feature_list_by_plugin(
                    registry, gst_plugin_get_name(plugin));
        for (GList *features = orig_features; features; features = g_list_next(features)) {
            if (G_UNLIKELY(features->data == NULL))
                continue;

            GstPluginFeature *feature = GST_PLUGIN_FEATURE(features->data);
            GstElementFactory *factory;

            if (GST_IS_TYPE_FIND_FACTORY(feature)) {
                iString name(gst_plugin_feature_get_name(feature));
                if (name.contains('/')) //filter out any string without '/' which is obviously not a mime type
                    supportedMimeTypes.insert(name.toLower());
                continue;
            } else if (!GST_IS_ELEMENT_FACTORY (feature)
                        || !(factory = GST_ELEMENT_FACTORY(gst_plugin_feature_load(feature)))) {
                continue;
            } else if (!isValidFactory(factory)) {
                // Do nothing
            } else for (const GList *pads = gst_element_factory_get_static_pad_templates(factory);
                        pads;
                        pads = g_list_next(pads)) {
                GstStaticPadTemplate *padtemplate = static_cast<GstStaticPadTemplate *>(pads->data);

                if (padtemplate->direction == GST_PAD_SINK && padtemplate->static_caps.string) {
                    GstCaps *caps = gst_static_caps_get(&padtemplate->static_caps);
                    if (gst_caps_is_any(caps) || gst_caps_is_empty(caps)) {
                    } else for (guint i = 0; i < gst_caps_get_size(caps); i++) {
                        GstStructure *structure = gst_caps_get_structure(caps, i);
                        iString nameLowcase = iString(gst_structure_get_name(structure)).toLower();

                        supportedMimeTypes.insert(nameLowcase);
                        if (nameLowcase.contains("mpeg")) {
                            //Because mpeg version number is only included in the detail
                            //description,  it is necessary to manually extract this information
                            //in order to match the mime type of mpeg4.
                            const GValue *value = gst_structure_get_value(structure, "mpegversion");
                            if (value) {
                                gchar *str = gst_value_serialize(value);
                                iString versions(str);
                                const QStringList elements = versions.split(QRegExp("\\D+"), iString::SkipEmptyParts);
                                for (const iString &e : elements)
                                    supportedMimeTypes.insert(nameLowcase + e);
                                g_free(str);
                            }
                        }
                    }
                }
            }
            gst_object_unref(factory);
        }
        gst_plugin_feature_list_free(orig_features);
    }
    gst_plugin_list_free (orig_plugins);

#if defined QT_SUPPORTEDMIMETYPES_DEBUG
    QStringList list = supportedMimeTypes.toList();
    list.sort();
    if (qgetenv("QT_DEBUG_PLUGINS").toInt() > 0) {
        for (const iString &type : qAsConst(list))
            qDebug() << type;
    }
#endif
    return supportedMimeTypes;
}

#if GST_CHECK_VERSION(1, 0, 0)
namespace {

struct ColorFormat { QImage::Format imageFormat; GstVideoFormat gstFormat; };
static const ColorFormat qt_colorLookup[] =
{
    { QImage::Format_RGBX8888, GST_VIDEO_FORMAT_RGBx  },
    { QImage::Format_RGBA8888, GST_VIDEO_FORMAT_RGBA  },
    { QImage::Format_RGB888  , GST_VIDEO_FORMAT_RGB   },
    { QImage::Format_RGB16   , GST_VIDEO_FORMAT_RGB16 }
};

}
#endif

#if GST_CHECK_VERSION(1,0,0)
QImage iGstUtils::bufferToImage(GstBuffer *buffer, const GstVideoInfo &videoInfo)
#else
QImage iGstUtils::bufferToImage(GstBuffer *buffer)
#endif
{
    QImage img;

#if GST_CHECK_VERSION(1,0,0)
    GstVideoInfo info = videoInfo;
    GstVideoFrame frame;
    if (!gst_video_frame_map(&frame, &info, buffer, GST_MAP_READ))
        return img;
#else
    GstCaps *caps = gst_buffer_get_caps(buffer);
    if (!caps)
        return img;

    GstStructure *structure = gst_caps_get_structure (caps, 0);
    gint width = 0;
    gint height = 0;

    if (!structure
            || !gst_structure_get_int(structure, "width", &width)
            || !gst_structure_get_int(structure, "height", &height)
            || width <= 0
            || height <= 0) {
        gst_caps_unref(caps);
        return img;
    }
    gst_caps_unref(caps);
#endif

#if GST_CHECK_VERSION(1,0,0)
    if (videoInfo.finfo->format == GST_VIDEO_FORMAT_I420) {
        const int width = videoInfo.width;
        const int height = videoInfo.height;

        const int stride[] = { frame.info.stride[0], frame.info.stride[1], frame.info.stride[2] };
        const uchar *data[] = {
            static_cast<const uchar *>(frame.data[0]),
            static_cast<const uchar *>(frame.data[1]),
            static_cast<const uchar *>(frame.data[2])
        };
#else
    if (istrcmp(gst_structure_get_name(structure), "video/x-raw-yuv") == 0) {
        const int stride[] = { width, width / 2, width / 2 };
        const uchar *data[] = {
            (const uchar *)buffer->data,
            (const uchar *)buffer->data + width * height,
            (const uchar *)buffer->data + width * height * 5 / 4
        };
#endif
        img = QImage(width/2, height/2, QImage::Format_RGB32);

        for (int y=0; y<height; y+=2) {
            const uchar *yLine = data[0] + (y * stride[0]);
            const uchar *uLine = data[1] + (y * stride[1] / 2);
            const uchar *vLine = data[2] + (y * stride[2] / 2);

            for (int x=0; x<width; x+=2) {
                const xreal Y = 1.164*(yLine[x]-16);
                const int U = uLine[x/2]-128;
                const int V = vLine[x/2]-128;

                int b = qBound(0, int(Y + 2.018*U), 255);
                int g = qBound(0, int(Y - 0.813*V - 0.391*U), 255);
                int r = qBound(0, int(Y + 1.596*V), 255);

                img.setPixel(x/2,y/2,qRgb(r,g,b));
            }
        }
#if GST_CHECK_VERSION(1,0,0)
    } else for (int i = 0; i < lengthOf(qt_colorLookup); ++i) {
        if (qt_colorLookup[i].gstFormat != videoInfo.finfo->format)
            continue;

        const QImage image(
                    static_cast<const uchar *>(frame.data[0]),
                    videoInfo.width,
                    videoInfo.height,
                    frame.info.stride[0],
                    qt_colorLookup[i].imageFormat);
        img = image;
        img.detach();

        break;
    }

    gst_video_frame_unmap(&frame);
#else
    } else if (istrcmp(gst_structure_get_name(structure), "video/x-raw-rgb") == 0) {
        QImage::Format format = QImage::Format_Invalid;
        int bpp = 0;
        gst_structure_get_int(structure, "bpp", &bpp);

        if (bpp == 24)
            format = QImage::Format_RGB888;
        else if (bpp == 32)
            format = QImage::Format_RGB32;

        if (format != QImage::Format_Invalid) {
            img = QImage((const uchar *)buffer->data,
                         width,
                         height,
                         format);
            img.bits(); //detach
        }
    }
#endif
    return img;
}


namespace {

#if GST_CHECK_VERSION(1,0,0)

struct VideoFormat
{
    QVideoFrame::PixelFormat pixelFormat;
    GstVideoFormat gstFormat;
};

static const VideoFormat qt_videoFormatLookup[] =
{
    { QVideoFrame::Format_YUV420P, GST_VIDEO_FORMAT_I420 },
    { QVideoFrame::Format_YV12   , GST_VIDEO_FORMAT_YV12 },
    { QVideoFrame::Format_UYVY   , GST_VIDEO_FORMAT_UYVY },
    { QVideoFrame::Format_YUYV   , GST_VIDEO_FORMAT_YUY2 },
    { QVideoFrame::Format_NV12   , GST_VIDEO_FORMAT_NV12 },
    { QVideoFrame::Format_NV21   , GST_VIDEO_FORMAT_NV21 },
    { QVideoFrame::Format_AYUV444, GST_VIDEO_FORMAT_AYUV },
#if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    { QVideoFrame::Format_RGB32 ,  GST_VIDEO_FORMAT_BGRx },
    { QVideoFrame::Format_BGR32 ,  GST_VIDEO_FORMAT_RGBx },
    { QVideoFrame::Format_ARGB32,  GST_VIDEO_FORMAT_BGRA },
    { QVideoFrame::Format_BGRA32,  GST_VIDEO_FORMAT_ARGB },
#else
    { QVideoFrame::Format_RGB32 ,  GST_VIDEO_FORMAT_xRGB },
    { QVideoFrame::Format_BGR32 ,  GST_VIDEO_FORMAT_xBGR },
    { QVideoFrame::Format_ARGB32,  GST_VIDEO_FORMAT_ARGB },
    { QVideoFrame::Format_BGRA32,  GST_VIDEO_FORMAT_BGRA },
#endif
    { QVideoFrame::Format_RGB24 ,  GST_VIDEO_FORMAT_RGB },
    { QVideoFrame::Format_BGR24 ,  GST_VIDEO_FORMAT_BGR },
    { QVideoFrame::Format_RGB565,  GST_VIDEO_FORMAT_RGB16 }
};

static int indexOfVideoFormat(QVideoFrame::PixelFormat format)
{
    for (int i = 0; i < lengthOf(qt_videoFormatLookup); ++i)
        if (qt_videoFormatLookup[i].pixelFormat == format)
            return i;

    return -1;
}

static int indexOfVideoFormat(GstVideoFormat format)
{
    for (int i = 0; i < lengthOf(qt_videoFormatLookup); ++i)
        if (qt_videoFormatLookup[i].gstFormat == format)
            return i;

    return -1;
}

#else

struct YuvFormat
{
    QVideoFrame::PixelFormat pixelFormat;
    guint32 fourcc;
    int bitsPerPixel;
};

static const YuvFormat qt_yuvColorLookup[] =
{
    { QVideoFrame::Format_YUV420P, GST_MAKE_FOURCC('I','4','2','0'), 8 },
    { QVideoFrame::Format_YV12,    GST_MAKE_FOURCC('Y','V','1','2'), 8 },
    { QVideoFrame::Format_UYVY,    GST_MAKE_FOURCC('U','Y','V','Y'), 16 },
    { QVideoFrame::Format_YUYV,    GST_MAKE_FOURCC('Y','U','Y','2'), 16 },
    { QVideoFrame::Format_NV12,    GST_MAKE_FOURCC('N','V','1','2'), 8 },
    { QVideoFrame::Format_NV21,    GST_MAKE_FOURCC('N','V','2','1'), 8 },
    { QVideoFrame::Format_AYUV444, GST_MAKE_FOURCC('A','Y','U','V'), 32 }
};

static int indexOfYuvColor(QVideoFrame::PixelFormat format)
{
    const int count = sizeof(qt_yuvColorLookup) / sizeof(YuvFormat);

    for (int i = 0; i < count; ++i)
        if (qt_yuvColorLookup[i].pixelFormat == format)
            return i;

    return -1;
}

static int indexOfYuvColor(guint32 fourcc)
{
    const int count = sizeof(qt_yuvColorLookup) / sizeof(YuvFormat);

    for (int i = 0; i < count; ++i)
        if (qt_yuvColorLookup[i].fourcc == fourcc)
            return i;

    return -1;
}

struct RgbFormat
{
    QVideoFrame::PixelFormat pixelFormat;
    int bitsPerPixel;
    int depth;
    int endianness;
    int red;
    int green;
    int blue;
    int alpha;
};

static const RgbFormat qt_rgbColorLookup[] =
{
    { QVideoFrame::Format_RGB32 , 32, 24, 4321, 0x0000FF00, 0x00FF0000, int(0xFF000000), 0x00000000 },
    { QVideoFrame::Format_RGB32 , 32, 24, 1234, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000 },
    { QVideoFrame::Format_BGR32 , 32, 24, 4321, int(0xFF000000), 0x00FF0000, 0x0000FF00, 0x00000000 },
    { QVideoFrame::Format_BGR32 , 32, 24, 1234, 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00000000 },
    { QVideoFrame::Format_ARGB32, 32, 24, 4321, 0x0000FF00, 0x00FF0000, int(0xFF000000), 0x000000FF },
    { QVideoFrame::Format_ARGB32, 32, 24, 1234, 0x00FF0000, 0x0000FF00, 0x000000FF, int(0xFF000000) },
    { QVideoFrame::Format_RGB24 , 24, 24, 4321, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000 },
    { QVideoFrame::Format_BGR24 , 24, 24, 4321, 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00000000 },
    { QVideoFrame::Format_RGB565, 16, 16, 1234, 0x0000F800, 0x000007E0, 0x0000001F, 0x00000000 }
};

static int indexOfRgbColor(
        int bits, int depth, int endianness, int red, int green, int blue, int alpha)
{
    const int count = sizeof(qt_rgbColorLookup) / sizeof(RgbFormat);

    for (int i = 0; i < count; ++i) {
        if (qt_rgbColorLookup[i].bitsPerPixel == bits
            && qt_rgbColorLookup[i].depth == depth
            && qt_rgbColorLookup[i].endianness == endianness
            && qt_rgbColorLookup[i].red == red
            && qt_rgbColorLookup[i].green == green
            && qt_rgbColorLookup[i].blue == blue
            && qt_rgbColorLookup[i].alpha == alpha) {
            return i;
        }
    }
    return -1;
}
#endif

}

#if GST_CHECK_VERSION(1,0,0)

QVideoSurfaceFormat iGstUtils::formatForCaps(
        GstCaps *caps, GstVideoInfo *info, QAbstractVideoBuffer::HandleType handleType)
{
    GstVideoInfo vidInfo;
    GstVideoInfo *infoPtr = info ? info : &vidInfo;

    if (gst_video_info_from_caps(infoPtr, caps)) {
        int index = indexOfVideoFormat(infoPtr->finfo->format);

        if (index != -1) {
            QVideoSurfaceFormat format(
                        iSize(infoPtr->width, infoPtr->height),
                        qt_videoFormatLookup[index].pixelFormat,
                        handleType);

            if (infoPtr->fps_d > 0)
                format.setFrameRate(xreal(infoPtr->fps_n) / infoPtr->fps_d);

            if (infoPtr->par_d > 0)
                format.setPixelAspectRatio(infoPtr->par_n, infoPtr->par_d);

            return format;
        }
    }
    return QVideoSurfaceFormat();
}

#else

QVideoSurfaceFormat iGstUtils::formatForCaps(
        GstCaps *caps, int *bytesPerLine, QAbstractVideoBuffer::HandleType handleType)
{
    const GstStructure *structure = gst_caps_get_structure(caps, 0);

    int bitsPerPixel = 0;
    iSize size = structureResolution(structure);
    QVideoFrame::PixelFormat pixelFormat = structurePixelFormat(structure, &bitsPerPixel);

    if (pixelFormat != QVideoFrame::Format_Invalid) {
        QVideoSurfaceFormat format(size, pixelFormat, handleType);

        QPair<xreal, xreal> rate = structureFrameRateRange(structure);
        if (rate.second)
            format.setFrameRate(rate.second);

        format.setPixelAspectRatio(structurePixelAspectRatio(structure));

        if (bytesPerLine)
            *bytesPerLine = ((size.width() * bitsPerPixel / 8) + 3) & ~3;

        return format;
    }
    return QVideoSurfaceFormat();
}

#endif

GstCaps *iGstUtils::capsForFormats(const QList<QVideoFrame::PixelFormat> &formats)
{
    GstCaps *caps = gst_caps_new_empty();

#if GST_CHECK_VERSION(1,0,0)
    for (QVideoFrame::PixelFormat format : formats) {
        int index = indexOfVideoFormat(format);

        if (index != -1) {
            gst_caps_append_structure(caps, gst_structure_new(
                    "video/x-raw",
                    "format"   , G_TYPE_STRING, gst_video_format_to_string(qt_videoFormatLookup[index].gstFormat),
                    NULL));
        }
    }
#else
    for (QVideoFrame::PixelFormat format : formats) {
        int index = indexOfYuvColor(format);

        if (index != -1) {
            gst_caps_append_structure(caps, gst_structure_new(
                    "video/x-raw-yuv",
                    "format", GST_TYPE_FOURCC, qt_yuvColorLookup[index].fourcc,
                    NULL));
            continue;
        }

        const int count = sizeof(qt_rgbColorLookup) / sizeof(RgbFormat);

        for (int i = 0; i < count; ++i) {
            if (qt_rgbColorLookup[i].pixelFormat == format) {
                GstStructure *structure = gst_structure_new(
                        "video/x-raw-rgb",
                        "bpp"       , G_TYPE_INT, qt_rgbColorLookup[i].bitsPerPixel,
                        "depth"     , G_TYPE_INT, qt_rgbColorLookup[i].depth,
                        "endianness", G_TYPE_INT, qt_rgbColorLookup[i].endianness,
                        "red_mask"  , G_TYPE_INT, qt_rgbColorLookup[i].red,
                        "green_mask", G_TYPE_INT, qt_rgbColorLookup[i].green,
                        "blue_mask" , G_TYPE_INT, qt_rgbColorLookup[i].blue,
                        NULL);

                if (qt_rgbColorLookup[i].alpha != 0) {
                    gst_structure_set(
                            structure, "alpha_mask", G_TYPE_INT, qt_rgbColorLookup[i].alpha, NULL);
                }
                gst_caps_append_structure(caps, structure);
            }
        }
    }
#endif

    gst_caps_set_simple(
                caps,
                "framerate", GST_TYPE_FRACTION_RANGE, 0, 1, INT_MAX, 1,
                "width"    , GST_TYPE_INT_RANGE, 1, INT_MAX,
                "height"   , GST_TYPE_INT_RANGE, 1, INT_MAX,
                NULL);

    return caps;
}

void iGstUtils::setFrameTimeStamps(QVideoFrame *frame, GstBuffer *buffer)
{
    // GStreamer uses nanoseconds, Qt uses microseconds
    xint64 startTime = GST_BUFFER_TIMESTAMP(buffer);
    if (startTime >= 0) {
        frame->setStartTime(startTime/G_GINT64_CONSTANT (1000));

        xint64 duration = GST_BUFFER_DURATION(buffer);
        if (duration >= 0)
            frame->setEndTime((startTime + duration)/G_GINT64_CONSTANT (1000));
    }
}

void iGstUtils::setMetaData(GstElement *element, const QMap<iByteArray, iVariant> &data)
{
    if (!GST_IS_TAG_SETTER(element))
        return;

    gst_tag_setter_reset_tags(GST_TAG_SETTER(element));

    QMapIterator<iByteArray, iVariant> it(data);
    while (it.hasNext()) {
        it.next();
        const iString tagName = it.key();
        const iVariant tagValue = it.value();

        switch (tagValue.type()) {
            case iVariant::String:
                gst_tag_setter_add_tags(GST_TAG_SETTER(element),
                    GST_TAG_MERGE_REPLACE,
                    tagName.toUtf8().constData(),
                    tagValue.toString().toUtf8().constData(),
                    NULL);
                break;
            case iVariant::Int:
            case iVariant::LongLong:
                gst_tag_setter_add_tags(GST_TAG_SETTER(element),
                    GST_TAG_MERGE_REPLACE,
                    tagName.toUtf8().constData(),
                    tagValue.toInt(),
                    NULL);
                break;
            case iVariant::Double:
                gst_tag_setter_add_tags(GST_TAG_SETTER(element),
                    GST_TAG_MERGE_REPLACE,
                    tagName.toUtf8().constData(),
                    tagValue.toDouble(),
                    NULL);
                break;
#if GST_CHECK_VERSION(0, 10, 31)
            case iVariant::DateTime: {
                QDateTime date = tagValue.toDateTime().toLocalTime();
                gst_tag_setter_add_tags(GST_TAG_SETTER(element),
                    GST_TAG_MERGE_REPLACE,
                    tagName.toUtf8().constData(),
                    gst_date_time_new_local_time(
                                date.date().year(), date.date().month(), date.date().day(),
                                date.time().hour(), date.time().minute(), date.time().second()),
                    NULL);
                break;
            }
#endif
            default:
                break;
        }
    }
}

void iGstUtils::setMetaData(GstBin *bin, const QMap<iByteArray, iVariant> &data)
{
    GstIterator *elements = gst_bin_iterate_all_by_interface(bin, GST_TYPE_TAG_SETTER);
#if GST_CHECK_VERSION(1,0,0)
    GValue item = G_VALUE_INIT;
    while (gst_iterator_next(elements, &item) == GST_ITERATOR_OK) {
        GstElement * const element = GST_ELEMENT(g_value_get_object(&item));
#else
    GstElement *element = 0;
    while (gst_iterator_next(elements, (void**)&element) == GST_ITERATOR_OK) {
#endif
        setMetaData(element, data);
    }
    gst_iterator_free(elements);
}


GstCaps *iGstUtils::videoFilterCaps()
{
    static GstStaticCaps staticCaps = GST_STATIC_CAPS(
#if GST_CHECK_VERSION(1,2,0)
        "video/x-raw(ANY);"
#elif GST_CHECK_VERSION(1,0,0)
        "video/x-raw;"
#else
        "video/x-raw-yuv;"
        "video/x-raw-rgb;"
        "video/x-raw-data;"
        "video/x-android-buffer;"
#endif
        "image/jpeg;"
        "video/x-h264");

    return gst_caps_make_writable(gst_static_caps_get(&staticCaps));
}

iSize iGstUtils::structureResolution(const GstStructure *s)
{
    iSize size;

    int w, h;
    if (s && gst_structure_get_int(s, "width", &w) && gst_structure_get_int(s, "height", &h)) {
        size.rwidth() = w;
        size.rheight() = h;
    }

    return size;
}

QVideoFrame::PixelFormat iGstUtils::structurePixelFormat(const GstStructure *structure, int *bpp)
{
    QVideoFrame::PixelFormat pixelFormat = QVideoFrame::Format_Invalid;

    if (!structure)
        return pixelFormat;

#if GST_CHECK_VERSION(1,0,0)

    if (gst_structure_has_name(structure, "video/x-raw")) {
        const gchar *s = gst_structure_get_string(structure, "format");
        if (s) {
            GstVideoFormat format = gst_video_format_from_string(s);
            int index = indexOfVideoFormat(format);

            if (index != -1)
                pixelFormat = qt_videoFormatLookup[index].pixelFormat;
        }
    }
#else
    if (istrcmp(gst_structure_get_name(structure), "video/x-raw-yuv") == 0) {
        guint32 fourcc = 0;
        gst_structure_get_fourcc(structure, "format", &fourcc);

        int index = indexOfYuvColor(fourcc);
        if (index != -1) {
            pixelFormat = qt_yuvColorLookup[index].pixelFormat;
            if (bpp)
                *bpp = qt_yuvColorLookup[index].bitsPerPixel;
        }
    } else if (istrcmp(gst_structure_get_name(structure), "video/x-raw-rgb") == 0) {
        int bitsPerPixel = 0;
        int depth = 0;
        int endianness = 0;
        int red = 0;
        int green = 0;
        int blue = 0;
        int alpha = 0;

        gst_structure_get_int(structure, "bpp", &bitsPerPixel);
        gst_structure_get_int(structure, "depth", &depth);
        gst_structure_get_int(structure, "endianness", &endianness);
        gst_structure_get_int(structure, "red_mask", &red);
        gst_structure_get_int(structure, "green_mask", &green);
        gst_structure_get_int(structure, "blue_mask", &blue);
        gst_structure_get_int(structure, "alpha_mask", &alpha);

        int index = indexOfRgbColor(bitsPerPixel, depth, endianness, red, green, blue, alpha);

        if (index != -1) {
            pixelFormat = qt_rgbColorLookup[index].pixelFormat;
            if (bpp)
                *bpp = qt_rgbColorLookup[index].bitsPerPixel;
        }
    }
#endif

    return pixelFormat;
}

iSize iGstUtils::structurePixelAspectRatio(const GstStructure *s)
{
    iSize ratio(1, 1);

    gint aspectNum = 0;
    gint aspectDenum = 0;
    if (s && gst_structure_get_fraction(s, "pixel-aspect-ratio", &aspectNum, &aspectDenum)) {
        if (aspectDenum > 0) {
            ratio.rwidth() = aspectNum;
            ratio.rheight() = aspectDenum;
        }
    }

    return ratio;
}

QPair<xreal, xreal> iGstUtils::structureFrameRateRange(const GstStructure *s)
{
    QPair<xreal, xreal> rate;

    if (!s)
        return rate;

    int n, d;
    if (gst_structure_get_fraction(s, "framerate", &n, &d)) {
        rate.second = xreal(n) / d;
        rate.first = rate.second;
    } else if (gst_structure_get_fraction(s, "max-framerate", &n, &d)) {
        rate.second = xreal(n) / d;
        if (gst_structure_get_fraction(s, "min-framerate", &n, &d))
            rate.first = xreal(n) / d;
        else
            rate.first = xreal(1);
    }

    return rate;
}

typedef QMap<iString, iString> FileExtensionMap;
Q_GLOBAL_STATIC(FileExtensionMap, fileExtensionMap)

iString iGstUtils::fileExtensionForMimeType(const iString &mimeType)
{
    if (fileExtensionMap->isEmpty()) {
        //extension for containers hard to guess from mimetype
        fileExtensionMap->insert("video/x-matroska", "mkv");
        fileExtensionMap->insert("video/quicktime", "mov");
        fileExtensionMap->insert("video/x-msvideo", "avi");
        fileExtensionMap->insert("video/msvideo", "avi");
        fileExtensionMap->insert("audio/mpeg", "mp3");
        fileExtensionMap->insert("application/x-shockwave-flash", "swf");
        fileExtensionMap->insert("application/x-pn-realmedia", "rm");
    }

    //for container names like avi instead of video/x-msvideo, use it as extension
    if (!mimeType.contains('/'))
        return mimeType;

    iString format = mimeType.left(mimeType.indexOf(','));
    iString extension = fileExtensionMap->value(format);

    if (!extension.isEmpty() || format.isEmpty())
        return extension;

    QRegExp rx("[-/]([\\w]+)$");

    if (rx.indexIn(format) != -1)
        extension = rx.cap(1);

    return extension;
}

void qt_gst_object_ref_sink(gpointer object)
{
#if GST_CHECK_VERSION(0,10,24)
    gst_object_ref_sink(object);
#else
    g_return_if_fail (GST_IS_OBJECT(object));

    GST_OBJECT_LOCK(object);
    if (G_LIKELY(GST_OBJECT_IS_FLOATING(object))) {
        GST_OBJECT_FLAG_UNSET(object, GST_OBJECT_FLOATING);
        GST_OBJECT_UNLOCK(object);
    } else {
        GST_OBJECT_UNLOCK(object);
        gst_object_ref(object);
    }
#endif
}

GstCaps *qt_gst_pad_get_current_caps(GstPad *pad)
{
#if GST_CHECK_VERSION(1,0,0)
    return gst_pad_get_current_caps(pad);
#else
    return gst_pad_get_negotiated_caps(pad);
#endif
}

GstCaps *qt_gst_pad_get_caps(GstPad *pad)
{
#if GST_CHECK_VERSION(1,0,0)
    return gst_pad_query_caps(pad, NULL);
#elif GST_CHECK_VERSION(0, 10, 26)
    return gst_pad_get_caps_reffed(pad);
#else
    return gst_pad_get_caps(pad);
#endif
}

GstStructure *qt_gst_structure_new_empty(const char *name)
{
#if GST_CHECK_VERSION(1,0,0)
    return gst_structure_new_empty(name);
#else
    return gst_structure_new(name, NULL);
#endif
}

gboolean qt_gst_element_query_position(GstElement *element, GstFormat format, gint64 *cur)
{
#if GST_CHECK_VERSION(1,0,0)
    return gst_element_query_position(element, format, cur);
#else
    return gst_element_query_position(element, &format, cur);
#endif
}

gboolean qt_gst_element_query_duration(GstElement *element, GstFormat format, gint64 *cur)
{
#if GST_CHECK_VERSION(1,0,0)
    return gst_element_query_duration(element, format, cur);
#else
    return gst_element_query_duration(element, &format, cur);
#endif
}

GstCaps *qt_gst_caps_normalize(GstCaps *caps)
{
#if GST_CHECK_VERSION(1,0,0)
    // gst_caps_normalize() takes ownership of the argument in 1.0
    return gst_caps_normalize(caps);
#else
    // in 0.10, it doesn't. Unref the argument to mimic the 1.0 behavior
    GstCaps *res = gst_caps_normalize(caps);
    gst_caps_unref(caps);
    return res;
#endif
}

const gchar *qt_gst_element_get_factory_name(GstElement *element)
{
    const gchar *name = 0;
    const GstElementFactory *factory = 0;

    if (element && (factory = gst_element_get_factory(element)))
        name = gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(factory));

    return name;
}

gboolean qt_gst_caps_can_intersect(const GstCaps * caps1, const GstCaps * caps2)
{
#if GST_CHECK_VERSION(0, 10, 25)
    return gst_caps_can_intersect(caps1, caps2);
#else
    GstCaps *intersection = gst_caps_intersect(caps1, caps2);
    gboolean res = !gst_caps_is_empty(intersection);
    gst_caps_unref(intersection);
    return res;
#endif
}

#if !GST_CHECK_VERSION(0, 10, 31)
static gboolean qt_gst_videosink_factory_filter(GstPluginFeature *feature, gpointer)
{
  guint rank;
  const gchar *klass;

  if (!GST_IS_ELEMENT_FACTORY(feature))
    return FALSE;

  klass = gst_element_factory_get_klass(GST_ELEMENT_FACTORY(feature));
  if (!(strstr(klass, "Sink") && strstr(klass, "Video")))
    return FALSE;

  rank = gst_plugin_feature_get_rank(feature);
  if (rank < GST_RANK_MARGINAL)
    return FALSE;

  return TRUE;
}

static gint qt_gst_compare_ranks(GstPluginFeature *f1, GstPluginFeature *f2)
{
  gint diff;

  diff = gst_plugin_feature_get_rank(f2) - gst_plugin_feature_get_rank(f1);
  if (diff != 0)
    return diff;

  return strcmp(gst_plugin_feature_get_name(f2), gst_plugin_feature_get_name (f1));
}
#endif

GList *qt_gst_video_sinks()
{
    GList *list = NULL;

#if GST_CHECK_VERSION(0, 10, 31)
    list = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_SINK | GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO,
                                                 GST_RANK_MARGINAL);
#else
    list = gst_registry_feature_filter(gst_registry_get_default(),
                                       (GstPluginFeatureFilter)qt_gst_videosink_factory_filter,
                                       FALSE, NULL);
    list
            QDebug operator <<(QDebug debug, GstCaps *caps); = g_list_sort(list, (GCompareFunc)qt_gst_compare_ranks);
#endif

    return list;
}

void qt_gst_util_double_to_fraction(gdouble src, gint *dest_n, gint *dest_d)
{
#if GST_CHECK_VERSION(0, 10, 26)
    gst_util_double_to_fraction(src, dest_n, dest_d);
#else
    qt_real_to_fraction(src, dest_n, dest_d);
#endif
}
#endif

} // namespace iShell
