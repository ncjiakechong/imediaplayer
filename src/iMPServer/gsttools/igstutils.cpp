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

#include <gst/audio/audio.h>
#include <gst/video/video.h>

#include "core/utils/isize.h"
#include "core/utils/iregexp.h"
#include "core/global/iglobalstatic.h"
#include "multimedia/video/ivideosurfaceformat.h"

#include "igstutils_p.h"

template<typename T, int N> static int lengthOf(const T (&)[N]) { return N; }

namespace iShell {

//internal
static void addTagToMap(const GstTagList *list,
                        const gchar *tag,
                        gpointer user_data)
{
    std::multimap<iByteArray, iVariant> *map = reinterpret_cast<std::multimap<iByteArray, iVariant>* >(user_data);

    GValue val;
    val.g_type = 0;
    gst_tag_list_copy_value(&val,list,tag);

    switch( G_VALUE_TYPE(&val) ) {
        case G_TYPE_STRING:
        {
            const gchar *str_value = g_value_get_string(&val);
            map->insert(std::pair<iByteArray, iVariant>(iByteArray(tag), iString::fromUtf8(str_value)));
            break;
        }
        case G_TYPE_INT:
            map->insert(std::pair<iByteArray, iVariant>(iByteArray(tag), g_value_get_int(&val)));
            break;
        case G_TYPE_UINT:
            map->insert(std::pair<iByteArray, iVariant>(iByteArray(tag), g_value_get_uint(&val)));
            break;
        case G_TYPE_LONG:
            map->insert(std::pair<iByteArray, iVariant>(iByteArray(tag), xint64(g_value_get_long(&val))));
            break;
        case G_TYPE_BOOLEAN:
            map->insert(std::pair<iByteArray, iVariant>(iByteArray(tag), g_value_get_boolean(&val)));
            break;
        case G_TYPE_CHAR:
        #if GLIB_CHECK_VERSION(2,32,0)
            map->insert(std::pair<iByteArray, iVariant>(iByteArray(tag), g_value_get_schar(&val)));
        #else
            map->insert(std::pair<iByteArray, iVariant>(iByteArray(tag), g_value_get_char(&val)));
        #endif
            break;
        case G_TYPE_DOUBLE:
            map->insert(std::pair<iByteArray, iVariant>(iByteArray(tag), g_value_get_double(&val)));
            break;
        default:
            // GST_TYPE_DATE is a function, not a constant, so pull it out of the switch
            #if GST_CHECK_VERSION(1,0,0)
            if (G_VALUE_TYPE(&val) == G_TYPE_DATE) {
                const GDate *date = (const GDate *)g_value_get_boxed(&val);
            #else
            if (G_VALUE_TYPE(&val) == GST_TYPE_DATE) {
                const GDate *date = gst_value_get_date(&val);
            #endif
                if (G_VALUE_TYPE(&val) == GST_TYPE_FRACTION) {
                    int nom = gst_value_get_fraction_numerator(&val);
                    int denom = gst_value_get_fraction_denominator(&val);

                    if (denom > 0) {
                        map->insert(std::pair<iByteArray, iVariant>(iByteArray(tag), double(nom)/denom));
                    }
                }
            }
            break;
    }

    g_value_unset(&val);
}

/*!
    \class iGstUtils
    \internal
*/

/*!
  Convert GstTagList structure to std::multimap<iByteArray, iVariant>.

  Mapping to int, bool, char, string, fractions and date are supported.
  Fraction values are converted to doubles.
*/
std::multimap<iByteArray, iVariant> iGstUtils::gstTagListToMap(const GstTagList *tags)
{
    std::multimap<iByteArray, iVariant> res;
    gst_tag_list_foreach(tags, addTagToMap, &res);

    return res;
}

/*!
  Returns resolution of \a caps.
  If caps doesn't have a valid size, an empty iSize is returned.
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
    iAudioFormat::SampleType sampleType;
    iAudioFormat::Endian byteOrder;
    int sampleSize;
};
static const AudioFormat qt_audioLookup[] =
{
    { GST_AUDIO_FORMAT_S8   , iAudioFormat::SignedInt  , iAudioFormat::LittleEndian, 8  },
    { GST_AUDIO_FORMAT_U8   , iAudioFormat::UnSignedInt, iAudioFormat::LittleEndian, 8  },
    { GST_AUDIO_FORMAT_S16LE, iAudioFormat::SignedInt  , iAudioFormat::LittleEndian, 16 },
    { GST_AUDIO_FORMAT_S16BE, iAudioFormat::SignedInt  , iAudioFormat::BigEndian   , 16 },
    { GST_AUDIO_FORMAT_U16LE, iAudioFormat::UnSignedInt, iAudioFormat::LittleEndian, 16 },
    { GST_AUDIO_FORMAT_U16BE, iAudioFormat::UnSignedInt, iAudioFormat::BigEndian   , 16 },
    { GST_AUDIO_FORMAT_S32LE, iAudioFormat::SignedInt  , iAudioFormat::LittleEndian, 32 },
    { GST_AUDIO_FORMAT_S32BE, iAudioFormat::SignedInt  , iAudioFormat::BigEndian   , 32 },
    { GST_AUDIO_FORMAT_U32LE, iAudioFormat::UnSignedInt, iAudioFormat::LittleEndian, 32 },
    { GST_AUDIO_FORMAT_U32BE, iAudioFormat::UnSignedInt, iAudioFormat::BigEndian   , 32 },
    { GST_AUDIO_FORMAT_S24LE, iAudioFormat::SignedInt  , iAudioFormat::LittleEndian, 24 },
    { GST_AUDIO_FORMAT_S24BE, iAudioFormat::SignedInt  , iAudioFormat::BigEndian   , 24 },
    { GST_AUDIO_FORMAT_U24LE, iAudioFormat::UnSignedInt, iAudioFormat::LittleEndian, 24 },
    { GST_AUDIO_FORMAT_U24BE, iAudioFormat::UnSignedInt, iAudioFormat::BigEndian   , 24 },
    { GST_AUDIO_FORMAT_F32LE, iAudioFormat::Float      , iAudioFormat::LittleEndian, 32 },
    { GST_AUDIO_FORMAT_F32BE, iAudioFormat::Float      , iAudioFormat::BigEndian   , 32 },
    { GST_AUDIO_FORMAT_F64LE, iAudioFormat::Float      , iAudioFormat::LittleEndian, 64 },
    { GST_AUDIO_FORMAT_F64BE, iAudioFormat::Float      , iAudioFormat::BigEndian   , 64 }
};

}
#endif

/*!
  Returns audio format for caps.
  If caps doesn't have a valid audio format, an empty iAudioFormat is returned.
*/

iAudioFormat iGstUtils::audioFormatForCaps(const GstCaps *caps)
{
    iAudioFormat format;
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
            format.setCodec(iStringLiteral("audio/pcm"));

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
            format.setByteOrder(iAudioFormat::LittleEndian);
        else if (endianness == 4321)
            format.setByteOrder(iAudioFormat::BigEndian);

        gboolean isSigned = FALSE;
        gst_structure_get_boolean(structure, "signed", &isSigned);
        if (isSigned)
            format.setSampleType(iAudioFormat::SignedInt);
        else
            format.setSampleType(iAudioFormat::UnSignedInt);

        // Number of bits allocated per sample.
        int width = 0;
        gst_structure_get_int(structure, "width", &width);

        // The number of bits used per sample. This must be less than or equal to the width.
        int depth = 0;
        gst_structure_get_int(structure, "depth", &depth);

        if (width != depth) {
            // Unsupported sample layout.
            return iAudioFormat();
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
            format.setByteOrder(iAudioFormat::LittleEndian);
        else if (endianness == 4321)
            format.setByteOrder(iAudioFormat::BigEndian);

        format.setSampleType(iAudioFormat::Float);

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
        return iAudioFormat();
    }
    #endif
    return format;
}

#if GST_CHECK_VERSION(1,0,0)
/*
  Returns audio format for a sample.
  If the buffer doesn't have a valid audio format, an empty iAudioFormat is returned.
*/
iAudioFormat iGstUtils::audioFormatForSample(GstSample *sample)
{
    GstCaps* caps = gst_sample_get_caps(sample);
    if (!caps)
        return iAudioFormat();

    return iGstUtils::audioFormatForCaps(caps);
}
#else
/*!
  Returns audio format for a buffer.
  If the buffer doesn't have a valid audio format, an empty iAudioFormat is returned.
*/
iAudioFormat iGstUtils::audioFormatForBuffer(GstBuffer *buffer)
{
    GstCaps* caps = gst_buffer_get_caps(buffer);
    if (!caps)
        return iAudioFormat();

    iAudioFormat format = iGstUtils::audioFormatForCaps(caps);
    gst_caps_unref(caps);
    return format;
}
#endif

/*!
  Builds GstCaps for an audio format.
  Returns 0 if the audio format is not valid.
  Caller must unref GstCaps.
*/

GstCaps *iGstUtils::capsForAudioFormat(const iAudioFormat &format)
{
    if (!format.isValid())
        return IX_NULLPTR;

    #if GST_CHECK_VERSION(1,0,0)
    const iAudioFormat::SampleType sampleType = format.sampleType();
    const iAudioFormat::Endian byteOrder = format.byteOrder();
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
                    IX_NULLPTR);
    }
    return IX_NULLPTR;
    #else
    GstStructure *structure = IX_NULLPTR;

    if (format.isValid()) {
        if (format.sampleType() == iAudioFormat::SignedInt || format.sampleType() == iAudioFormat::UnSignedInt) {
            structure = gst_structure_new("audio/x-raw-int", IX_NULLPTR);
        } else if (format.sampleType() == iAudioFormat::Float) {
            structure = gst_structure_new("audio/x-raw-float", IX_NULLPTR);
        }
    }

    GstCaps *caps = IX_NULLPTR;

    if (structure) {
        gst_structure_set(structure, "rate", G_TYPE_INT, format.sampleRate(), IX_NULLPTR);
        gst_structure_set(structure, "channels", G_TYPE_INT, format.channelCount(), IX_NULLPTR);
        gst_structure_set(structure, "width", G_TYPE_INT, format.sampleSize(), IX_NULLPTR);
        gst_structure_set(structure, "depth", G_TYPE_INT, format.sampleSize(), IX_NULLPTR);

        if (format.byteOrder() == iAudioFormat::LittleEndian)
            gst_structure_set(structure, "endianness", G_TYPE_INT, 1234, IX_NULLPTR);
        else if (format.byteOrder() == iAudioFormat::BigEndian)
            gst_structure_set(structure, "endianness", G_TYPE_INT, 4321, IX_NULLPTR);

        if (format.sampleType() == iAudioFormat::SignedInt)
            gst_structure_set(structure, "signed", G_TYPE_BOOLEAN, TRUE, IX_NULLPTR);
        else if (format.sampleType() == iAudioFormat::UnSignedInt)
            gst_structure_set(structure, "signed", G_TYPE_BOOLEAN, FALSE, IX_NULLPTR);

        caps = gst_caps_new_empty();
        IX_ASSERT(caps);
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
        gst_init(IX_NULLPTR, IX_NULLPTR);
    }
}

namespace {
    const char* getCodecAlias(const iString &codec)
    {
        if (codec.startsWith(iLatin1String("avc1.")))
            return "video/x-h264";

        if (codec.startsWith(iLatin1String("mp4a.")))
            return "audio/mpeg4";

        if (codec.startsWith(iLatin1String("mp4v.20.")))
            return "video/mpeg4";

        if (codec == iLatin1String("samr"))
            return "audio/amr";

        return IX_NULLPTR;
    }

    const char* getMimeTypeAlias(const iString &mimeType)
    {
        if (mimeType == iLatin1String("video/mp4"))
            return "video/mpeg4";

        if (mimeType == iLatin1String("audio/mp4"))
            return "audio/mpeg4";

        if (mimeType == iLatin1String("video/ogg")
            || mimeType == iLatin1String("audio/ogg"))
            return "application/ogg";

        return IX_NULLPTR;
    }
}

iMultimedia::SupportEstimate iGstUtils::hasSupport(const iString &mimeType,
                                                    const std::list<iString> &codecs,
                                                    const std::unordered_set<iString, iKeyHashFunc> &supportedMimeTypeSet)
{
    if (supportedMimeTypeSet.empty())
        return iMultimedia::NotSupported;

    iString mimeTypeLowcase = mimeType.toLower();
    bool containsMimeType = (supportedMimeTypeSet.find(mimeTypeLowcase) != supportedMimeTypeSet.cend());
    if (!containsMimeType) {
        const char* mimeTypeAlias = getMimeTypeAlias(mimeTypeLowcase);
        containsMimeType = (supportedMimeTypeSet.find(iLatin1String(mimeTypeAlias)) != supportedMimeTypeSet.cend());
        if (!containsMimeType) {
            containsMimeType = (supportedMimeTypeSet.find(iLatin1String("video/") + mimeTypeLowcase) != supportedMimeTypeSet.cend())
                               || (supportedMimeTypeSet.find(iLatin1String("video/x-") + mimeTypeLowcase) != supportedMimeTypeSet.cend())
                               || (supportedMimeTypeSet.find(iLatin1String("audio/") + mimeTypeLowcase) != supportedMimeTypeSet.cend())
                               || (supportedMimeTypeSet.find(iLatin1String("audio/x-") + mimeTypeLowcase) != supportedMimeTypeSet.cend());
        }
    }

    int supportedCodecCount = 0;
    for (const iString &codec : codecs) {
        iString codecLowcase = codec.toLower();
        const char* codecAlias = getCodecAlias(codecLowcase);
        if (codecAlias) {
            if (supportedMimeTypeSet.find(iLatin1String(codecAlias)) != supportedMimeTypeSet.cend())
                supportedCodecCount++;
        } else if ((supportedMimeTypeSet.find(iLatin1String("video/") + codecLowcase) != supportedMimeTypeSet.cend())
                   || (supportedMimeTypeSet.find(iLatin1String("video/x-") + codecLowcase) != supportedMimeTypeSet.cend())
                   || (supportedMimeTypeSet.find(iLatin1String("audio/") + codecLowcase) != supportedMimeTypeSet.cend())
                   || (supportedMimeTypeSet.find(iLatin1String("audio/x-") + codecLowcase) != supportedMimeTypeSet.cend())) {
            supportedCodecCount++;
        }
    }
    if (supportedCodecCount > 0 && supportedCodecCount == codecs.size())
        return iMultimedia::ProbablySupported;

    if (supportedCodecCount == 0 && !containsMimeType)
        return iMultimedia::NotSupported;

    return iMultimedia::MaybeSupported;
}


std::unordered_set<iString, iKeyHashFunc> iGstUtils::supportedMimeTypes(bool (*isValidFactory)(GstElementFactory *factory))
{
    std::unordered_set<iString, iKeyHashFunc> supportedMimeTypes;

    //enumerate supported mime types
    gst_init(IX_NULLPTR, IX_NULLPTR);

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
            if (G_UNLIKELY(features->data == IX_NULLPTR))
                continue;

            GstPluginFeature *feature = GST_PLUGIN_FEATURE(features->data);
            GstElementFactory *factory;

            if (GST_IS_TYPE_FIND_FACTORY(feature)) {
                iString name(iLatin1String(gst_plugin_feature_get_name(feature)));
                if (name.contains(iLatin1Char('/'))) //filter out any string without '/' which is obviously not a mime type
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
                        iString nameLowcase = iString::fromLatin1(gst_structure_get_name(structure)).toLower();

                        supportedMimeTypes.insert(nameLowcase);
                        if (nameLowcase.contains(iLatin1String("mpeg"))) {
                            //Because mpeg version number is only included in the detail
                            //description,  it is necessary to manually extract this information
                            //in order to match the mime type of mpeg4.
                            const GValue *value = gst_structure_get_value(structure, "mpegversion");
                            if (value) {
                                gchar *str = gst_value_serialize(value);
                                iString versions = iLatin1String(str);
                                const std::list<iString> elements = versions.split(iRegExp(iLatin1String("\\D+")), iString::SkipEmptyParts);
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

    return supportedMimeTypes;
}

namespace {

#if GST_CHECK_VERSION(1,0,0)

struct VideoFormat
{
    iVideoFrame::PixelFormat pixelFormat;
    GstVideoFormat gstFormat;
};

static const VideoFormat ix_videoFormatLookup[] =
{
    { iVideoFrame::Format_YUV420P, GST_VIDEO_FORMAT_I420 },
    { iVideoFrame::Format_YV12   , GST_VIDEO_FORMAT_YV12 },
    { iVideoFrame::Format_UYVY   , GST_VIDEO_FORMAT_UYVY },
    { iVideoFrame::Format_YUYV   , GST_VIDEO_FORMAT_YUY2 },
    { iVideoFrame::Format_NV12   , GST_VIDEO_FORMAT_NV12 },
    { iVideoFrame::Format_NV21   , GST_VIDEO_FORMAT_NV21 },
    { iVideoFrame::Format_AYUV444, GST_VIDEO_FORMAT_AYUV },
    #if Q_BYTE_ORDER == Q_LITTLE_ENDIAN
    { iVideoFrame::Format_RGB32 ,  GST_VIDEO_FORMAT_BGRx },
    { iVideoFrame::Format_BGR32 ,  GST_VIDEO_FORMAT_RGBx },
    { iVideoFrame::Format_ARGB32,  GST_VIDEO_FORMAT_BGRA },
    { iVideoFrame::Format_BGRA32,  GST_VIDEO_FORMAT_ARGB },
    #else
    { iVideoFrame::Format_RGB32 ,  GST_VIDEO_FORMAT_xRGB },
    { iVideoFrame::Format_BGR32 ,  GST_VIDEO_FORMAT_xBGR },
    { iVideoFrame::Format_ARGB32,  GST_VIDEO_FORMAT_ARGB },
    { iVideoFrame::Format_BGRA32,  GST_VIDEO_FORMAT_BGRA },
    #endif
    { iVideoFrame::Format_RGB24 ,  GST_VIDEO_FORMAT_RGB },
    { iVideoFrame::Format_BGR24 ,  GST_VIDEO_FORMAT_BGR },
    { iVideoFrame::Format_RGB565,  GST_VIDEO_FORMAT_RGB16 }
};

static int indexOfVideoFormat(iVideoFrame::PixelFormat format)
{
    for (int i = 0; i < lengthOf(ix_videoFormatLookup); ++i)
        if (ix_videoFormatLookup[i].pixelFormat == format)
            return i;

    return -1;
}

static int indexOfVideoFormat(GstVideoFormat format)
{
    for (int i = 0; i < lengthOf(ix_videoFormatLookup); ++i)
        if (ix_videoFormatLookup[i].gstFormat == format)
            return i;

    return -1;
}

#else

struct YuvFormat
{
    iVideoFrame::PixelFormat pixelFormat;
    guint32 fourcc;
    int bitsPerPixel;
};

static const YuvFormat qt_yuvColorLookup[] =
{
    { iVideoFrame::Format_YUV420P, GST_MAKE_FOURCC('I','4','2','0'), 8 },
    { iVideoFrame::Format_YV12,    GST_MAKE_FOURCC('Y','V','1','2'), 8 },
    { iVideoFrame::Format_UYVY,    GST_MAKE_FOURCC('U','Y','V','Y'), 16 },
    { iVideoFrame::Format_YUYV,    GST_MAKE_FOURCC('Y','U','Y','2'), 16 },
    { iVideoFrame::Format_NV12,    GST_MAKE_FOURCC('N','V','1','2'), 8 },
    { iVideoFrame::Format_NV21,    GST_MAKE_FOURCC('N','V','2','1'), 8 },
    { iVideoFrame::Format_AYUV444, GST_MAKE_FOURCC('A','Y','U','V'), 32 }
};

static int indexOfYuvColor(iVideoFrame::PixelFormat format)
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
    iVideoFrame::PixelFormat pixelFormat;
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
    { iVideoFrame::Format_RGB32 , 32, 24, 4321, 0x0000FF00, 0x00FF0000, int(0xFF000000), 0x00000000 },
    { iVideoFrame::Format_RGB32 , 32, 24, 1234, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000 },
    { iVideoFrame::Format_BGR32 , 32, 24, 4321, int(0xFF000000), 0x00FF0000, 0x0000FF00, 0x00000000 },
    { iVideoFrame::Format_BGR32 , 32, 24, 1234, 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00000000 },
    { iVideoFrame::Format_ARGB32, 32, 24, 4321, 0x0000FF00, 0x00FF0000, int(0xFF000000), 0x000000FF },
    { iVideoFrame::Format_ARGB32, 32, 24, 1234, 0x00FF0000, 0x0000FF00, 0x000000FF, int(0xFF000000) },
    { iVideoFrame::Format_RGB24 , 24, 24, 4321, 0x00FF0000, 0x0000FF00, 0x000000FF, 0x00000000 },
    { iVideoFrame::Format_BGR24 , 24, 24, 4321, 0x000000FF, 0x0000FF00, 0x00FF0000, 0x00000000 },
    { iVideoFrame::Format_RGB565, 16, 16, 1234, 0x0000F800, 0x000007E0, 0x0000001F, 0x00000000 }
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

iVideoSurfaceFormat iGstUtils::formatForCaps(
        GstCaps *caps, GstVideoInfo *info, iAbstractVideoBuffer::HandleType handleType)
{
    GstVideoInfo vidInfo;
    GstVideoInfo *infoPtr = info ? info : &vidInfo;

    if (gst_video_info_from_caps(infoPtr, caps)) {
        int index = indexOfVideoFormat(infoPtr->finfo->format);

        if (index != -1) {
            iVideoSurfaceFormat format(
                        iSize(infoPtr->width, infoPtr->height),
                        ix_videoFormatLookup[index].pixelFormat,
                        handleType);

            if (infoPtr->fps_d > 0)
                format.setFrameRate(xreal(infoPtr->fps_n) / infoPtr->fps_d);

            if (infoPtr->par_d > 0)
                format.setPixelAspectRatio(infoPtr->par_n, infoPtr->par_d);

            return format;
        }
    }
    return iVideoSurfaceFormat();
}

#else

iVideoSurfaceFormat iGstUtils::formatForCaps(
        GstCaps *caps, int *bytesPerLine, iAbstractVideoBuffer::HandleType handleType)
{
    const GstStructure *structure = gst_caps_get_structure(caps, 0);

    int bitsPerPixel = 0;
    iSize size = structureResolution(structure);
    iVideoFrame::PixelFormat pixelFormat = structurePixelFormat(structure, &bitsPerPixel);

    if (pixelFormat != iVideoFrame::Format_Invalid) {
        iVideoSurfaceFormat format(size, pixelFormat, handleType);

        std::pair<xreal, xreal> rate = structureFrameRateRange(structure);
        if (rate.second)
            format.setFrameRate(rate.second);

        format.setPixelAspectRatio(structurePixelAspectRatio(structure));

        if (bytesPerLine)
            *bytesPerLine = ((size.width() * bitsPerPixel / 8) + 3) & ~3;

        return format;
    }
    return iVideoSurfaceFormat();
}

#endif

GstCaps *iGstUtils::capsForFormats(const std::list<iVideoFrame::PixelFormat> &formats)
{
    GstCaps *caps = gst_caps_new_empty();

#if GST_CHECK_VERSION(1,0,0)
    for (iVideoFrame::PixelFormat format : formats) {
        int index = indexOfVideoFormat(format);

        if (index != -1) {
            gst_caps_append_structure(caps, gst_structure_new(
                    "video/x-raw",
                    "format"   , G_TYPE_STRING, gst_video_format_to_string(ix_videoFormatLookup[index].gstFormat),
                    IX_NULLPTR));
        }
    }
#else
    for (iVideoFrame::PixelFormat format : formats) {
        int index = indexOfYuvColor(format);

        if (index != -1) {
            gst_caps_append_structure(caps, gst_structure_new(
                    "video/x-raw-yuv",
                    "format", GST_TYPE_FOURCC, qt_yuvColorLookup[index].fourcc,
                    IX_NULLPTR));
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
                        IX_NULLPTR);

                if (qt_rgbColorLookup[i].alpha != 0) {
                    gst_structure_set(
                            structure, "alpha_mask", G_TYPE_INT, qt_rgbColorLookup[i].alpha, IX_NULLPTR);
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
                IX_NULLPTR);

    return caps;
}

void iGstUtils::setFrameTimeStamps(iVideoFrame *frame, GstBuffer *buffer)
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

void iGstUtils::setMetaData(GstElement *element, const std::multimap<iByteArray, iVariant> &data)
{
    if (!GST_IS_TAG_SETTER(element))
        return;

    gst_tag_setter_reset_tags(GST_TAG_SETTER(element));

    std::multimap<iByteArray, iVariant>::const_iterator it = data.cbegin();
    for (it = data.cbegin(); it != data.cend(); ++it) {
        const iString tagName = iString::fromLatin1(it->first);
        const iVariant tagValue = it->second;

        if (tagValue.type() == iVariant::iMetaTypeId<iString>(0)) {
            gst_tag_setter_add_tags(GST_TAG_SETTER(element),
                GST_TAG_MERGE_REPLACE,
                tagName.toUtf8().constData(),
                tagValue.value<iString>().toUtf8().constData(),
                IX_NULLPTR);
        } if (tagValue.type() == iVariant::iMetaTypeId<int>(0)
              || tagValue.type() == iVariant::iMetaTypeId<long long>(0)) {
            gst_tag_setter_add_tags(GST_TAG_SETTER(element),
                GST_TAG_MERGE_REPLACE,
                tagName.toUtf8().constData(),
                tagValue.value<int>(),
                IX_NULLPTR);
        } if (tagValue.type() == iVariant::iMetaTypeId<double>(0)) {
            gst_tag_setter_add_tags(GST_TAG_SETTER(element),
                GST_TAG_MERGE_REPLACE,
                tagName.toUtf8().constData(),
                tagValue.value<double>(),
                IX_NULLPTR);
        } else {}
    }
}

void iGstUtils::setMetaData(GstBin *bin, const std::multimap<iByteArray, iVariant> &data)
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
    const char *caps =
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
        "video/x-h264";
    static GstStaticCaps staticCaps = GST_STATIC_CAPS(caps);

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

iVideoFrame::PixelFormat iGstUtils::structurePixelFormat(const GstStructure *structure, int *bpp)
{
    iVideoFrame::PixelFormat pixelFormat = iVideoFrame::Format_Invalid;

    if (!structure)
        return pixelFormat;

    #if GST_CHECK_VERSION(1,0,0)

    if (gst_structure_has_name(structure, "video/x-raw")) {
        const gchar *s = gst_structure_get_string(structure, "format");
        if (s) {
            GstVideoFormat format = gst_video_format_from_string(s);
            int index = indexOfVideoFormat(format);

            if (index != -1)
                pixelFormat = ix_videoFormatLookup[index].pixelFormat;
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

std::pair<xreal, xreal> iGstUtils::structureFrameRateRange(const GstStructure *s)
{
    std::pair<xreal, xreal> rate;

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

typedef std::multimap<iString, iString> FileExtensionMap;
IX_GLOBAL_STATIC(FileExtensionMap, fileExtensionMap)

iString iGstUtils::fileExtensionForMimeType(const iString &mimeType)
{
    if (fileExtensionMap->empty()) {
        //extension for containers hard to guess from mimetype
        fileExtensionMap->insert(std::pair<iString, iString>(iStringLiteral("video/x-matroska"), iLatin1String("mkv")));
        fileExtensionMap->insert(std::pair<iString, iString>(iStringLiteral("video/quicktime"), iLatin1String("mov")));
        fileExtensionMap->insert(std::pair<iString, iString>(iStringLiteral("video/x-msvideo"), iLatin1String("avi")));
        fileExtensionMap->insert(std::pair<iString, iString>(iStringLiteral("video/msvideo"), iLatin1String("avi")));
        fileExtensionMap->insert(std::pair<iString, iString>(iStringLiteral("audio/mpeg"), iLatin1String("mp3")));
        fileExtensionMap->insert(std::pair<iString, iString>(iStringLiteral("application/x-shockwave-flash"), iLatin1String("swf")));
        fileExtensionMap->insert(std::pair<iString, iString>(iStringLiteral("application/x-pn-realmedia"), iLatin1String("rm")));
    }

    //for container names like avi instead of video/x-msvideo, use it as extension
    if (!mimeType.contains(iLatin1Char('/')))
        return mimeType;

    iString format = mimeType.left(mimeType.indexOf(iLatin1Char(',')));
    std::multimap<iString, iString>::const_iterator it = fileExtensionMap->find(format);
    iString extension;
    if (it != fileExtensionMap->cend())
        extension = it->second;

    if (!extension.isEmpty() || format.isEmpty())
        return extension;

    iRegExp rx(iStringLiteral("[-/]([\\w]+)$"));
    rx.exactMatch(format);
    if (rx.captureCount() > 0)
        extension = rx.capturedTexts().front();

    return extension;
}

#if GST_CHECK_VERSION(0,10,30)
iVariant iGstUtils::fromGStreamerOrientation(const iVariant &value)
{
    // Note gstreamer tokens either describe the counter clockwise rotation of the
    // image or the clockwise transform to apply to correct the image.  The orientation
    // value returned is the clockwise rotation of the image.
    const iString token = value.value<iString>();
    if (token == iStringLiteral("rotate-90"))
        return 270;
    if (token == iStringLiteral("rotate-180"))
        return 180;
    if (token == iStringLiteral("rotate-270"))
        return 90;
    return 0;
}

iVariant iGstUtils::toGStreamerOrientation(const iVariant &value)
{
    switch (value.value<int>()) {
    case 90:
        return iStringLiteral("rotate-270");
    case 180:
        return iStringLiteral("rotate-180");
    case 270:
        return iStringLiteral("rotate-90");
    default:
        return iStringLiteral("rotate-0");
    }
}
#endif

void ix_gst_object_ref_sink(gpointer object)
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

GstCaps *ix_gst_pad_get_current_caps(GstPad *pad)
{
    #if GST_CHECK_VERSION(1,0,0)
    return gst_pad_get_current_caps(pad);
    #else
    return gst_pad_get_negotiated_caps(pad);
    #endif
}

GstCaps *ix_gst_pad_get_caps(GstPad *pad)
{
    #if GST_CHECK_VERSION(1,0,0)
    return gst_pad_query_caps(pad, IX_NULLPTR);
    #elif GST_CHECK_VERSION(0, 10, 26)
    return gst_pad_get_caps_reffed(pad);
    #else
    return gst_pad_get_caps(pad);
    #endif
}

GstStructure *ix_gst_structure_new_empty(const char *name)
{
    #if GST_CHECK_VERSION(1,0,0)
    return gst_structure_new_empty(name);
    #else
    return gst_structure_new(name, IX_NULLPTR);
    #endif
}

gboolean ix_gst_element_query_position(GstElement *element, GstFormat format, gint64 *cur)
{
    #if GST_CHECK_VERSION(1,0,0)
    return gst_element_query_position(element, format, cur);
    #else
    return gst_element_query_position(element, &format, cur);
    #endif
}

gboolean ix_gst_element_query_duration(GstElement *element, GstFormat format, gint64 *cur)
{
    #if GST_CHECK_VERSION(1,0,0)
    return gst_element_query_duration(element, format, cur);
    #else
    return gst_element_query_duration(element, &format, cur);
    #endif
}

GstCaps *ix_gst_caps_normalize(GstCaps *caps)
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

const gchar *ix_gst_element_get_factory_name(GstElement *element)
{
    const gchar *name = 0;
    const GstElementFactory *factory = 0;

    if (element && (factory = gst_element_get_factory(element)))
        name = gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(factory));

    return name;
}

gboolean ix_gst_caps_can_intersect(const GstCaps * caps1, const GstCaps * caps2)
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

GList *ix_gst_video_sinks()
{
    GList *list = IX_NULLPTR;

    #if GST_CHECK_VERSION(0, 10, 31)
    list = gst_element_factory_list_get_elements(GST_ELEMENT_FACTORY_TYPE_SINK | GST_ELEMENT_FACTORY_TYPE_MEDIA_VIDEO,
                                                 GST_RANK_MARGINAL);
    #else
    list = gst_registry_feature_filter(gst_registry_get_default(),
                                       (GstPluginFeatureFilter)qt_gst_videosink_factory_filter,
                                       FALSE, IX_NULLPTR);
    list = g_list_sort(list, (GCompareFunc)qt_gst_compare_ranks);
    #endif

    return list;
}

void ix_real_to_fraction(xreal value, int *numerator, int *denominator)
{
    if (!numerator || !denominator)
        return;

    const int dMax = 1000;
    int n1 = 0, d1 = 1, n2 = 1, d2 = 1;
    xreal mid = 0.;
    while (d1 <= dMax && d2 <= dMax) {
        mid = xreal(n1 + n2) / (d1 + d2);

        if (std::abs(value - mid) < 0.000001) {
            if (d1 + d2 <= dMax) {
                *numerator = n1 + n2;
                *denominator = d1 + d2;
                return;
            } else if (d2 > d1) {
                *numerator = n2;
                *denominator = d2;
                return;
            } else {
                *numerator = n1;
                *denominator = d1;
                return;
            }
        } else if (value > mid) {
            n1 = n1 + n2;
            d1 = d1 + d2;
        } else {
            n2 = n1 + n2;
            d2 = d1 + d2;
        }
    }

    if (d1 > dMax) {
        *numerator = n2;
        *denominator = d2;
    } else {
        *numerator = n1;
        *denominator = d1;
    }
}

void ix_gst_util_double_to_fraction(gdouble src, gint *dest_n, gint *dest_d)
{
    #if GST_CHECK_VERSION(0, 10, 26)
    gst_util_double_to_fraction(src, dest_n, dest_d);
    #else
    ix_real_to_fraction(src, dest_n, dest_d);
    #endif
}

} // namespace iShell
