/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstcodecsinfo.cpp
/// @brief   Short description
/// @details description.
/// @version 1.0
/// @author  anfengce@
/////////////////////////////////////////////////////////////////

#include <set>
#include <gst/pbutils/pbutils.h>

#include "igstcodecsinfo_p.h"
#include "igstutils_p.h"

namespace iShell {

iGstCodecsInfo::iGstCodecsInfo(iGstCodecsInfo::ElementType elementType)
{
    updateCodecs(elementType);
}

std::list<iString> iGstCodecsInfo::supportedCodecs() const
{
    return m_codecs;
}

iString iGstCodecsInfo::codecDescription(const iString &codec) const
{
    return m_codecInfo.find(codec)->second.description;
}

iByteArray iGstCodecsInfo::codecElement(const iString &codec) const

{
    return m_codecInfo.find(codec)->second.elementName;
}

std::list<iString> iGstCodecsInfo::codecOptions(const iString &codec) const
{
    std::list<iString> options;

    iByteArray elementName = m_codecInfo.find(codec)->second.elementName;
    if (elementName.isEmpty())
        return options;

    GstElement *element = gst_element_factory_make(elementName.constData(), IX_NULLPTR);
    if (element) {
        guint numProperties;
        GParamSpec **properties = g_object_class_list_properties(G_OBJECT_GET_CLASS(element),
                                                                 &numProperties);
        for (guint j = 0; j < numProperties; ++j) {
            GParamSpec *property = properties[j];
            // ignore some properties
            if (strcmp(property->name, "name") == 0 || strcmp(property->name, "parent") == 0)
                continue;

            options.push_back(iLatin1String(property->name));
        }
        g_free(properties);
        gst_object_unref(element);
    }

    return options;
}

void iGstCodecsInfo::updateCodecs(ElementType elementType)
{
    m_codecs.clear();
    m_codecInfo.clear();

    GList *elements = elementFactories(elementType);

    std::unordered_set<iByteArray, iKeyHashFunc> fakeEncoderMimeTypes;
    fakeEncoderMimeTypes.insert("unknown/unknown");
    fakeEncoderMimeTypes.insert("audio/x-raw-int");
    fakeEncoderMimeTypes.insert("audio/x-raw-float");
    fakeEncoderMimeTypes.insert("video/x-raw-yuv");
    fakeEncoderMimeTypes.insert("video/x-raw-rgb");

    std::unordered_set<iByteArray, iKeyHashFunc> fieldsToAdd;
    fieldsToAdd.insert("mpegversion");
    fieldsToAdd.insert("layer");
    fieldsToAdd.insert("layout");
    fieldsToAdd.insert("raversion");
    fieldsToAdd.insert("wmaversion");
    fieldsToAdd.insert("wmvversion");
    fieldsToAdd.insert("variant");
    fieldsToAdd.insert("systemstream");

    GList *element = elements;
    while (element) {
        GstElementFactory *factory = (GstElementFactory *)element->data;
        element = element->next;

        const GList *padTemplates = gst_element_factory_get_static_pad_templates(factory);
        while (padTemplates) {
            GstStaticPadTemplate *padTemplate = (GstStaticPadTemplate *)padTemplates->data;
            padTemplates = padTemplates->next;

            if (padTemplate->direction == GST_PAD_SRC) {
                GstCaps *caps = gst_static_caps_get(&padTemplate->static_caps);
                for (uint i=0; i<gst_caps_get_size(caps); i++) {
                    const GstStructure *structure = gst_caps_get_structure(caps, i);

                    //skip "fake" encoders
                    if (fakeEncoderMimeTypes.end() != fakeEncoderMimeTypes.find(gst_structure_get_name(structure)))
                        continue;

                    GstStructure *newStructure = ix_gst_structure_new_empty(gst_structure_get_name(structure));

                    //add structure fields to distinguish between formats with similar mime types,
                    //like audio/mpeg
                    for (int j=0; j<gst_structure_n_fields(structure); j++) {
                        const gchar* fieldName = gst_structure_nth_field_name(structure, j);
                        if (fieldsToAdd.end() != fieldsToAdd.find(fieldName)) {
                            const GValue *value = gst_structure_get_value(structure, fieldName);
                            GType valueType = G_VALUE_TYPE(value);

                            //don't add values of range type,
                            //gst_pb_utils_get_codec_description complains about not fixed caps

                            if (valueType != GST_TYPE_INT_RANGE && valueType != GST_TYPE_DOUBLE_RANGE &&
                                valueType != GST_TYPE_FRACTION_RANGE && valueType != GST_TYPE_LIST &&
                                valueType != GST_TYPE_ARRAY)
                                gst_structure_set_value(newStructure, fieldName, value);
                        }
                    }

                    GstCaps *newCaps = gst_caps_new_full(newStructure, IX_NULLPTR);

                    gchar *capsString = gst_caps_to_string(newCaps);
                    iString codec = iLatin1String(capsString);
                    if (capsString)
                        g_free(capsString);
                    GstRank rank = GstRank(gst_plugin_feature_get_rank(GST_PLUGIN_FEATURE(factory)));

                    // If two elements provide the same codec, use the highest ranked one
                    std::multimap<iString, CodecInfo>::const_iterator it = m_codecInfo.find(codec);
                    if (it == m_codecInfo.cend() || it->second.rank < rank) {
                        if (it == m_codecInfo.cend())
                            m_codecs.push_back(codec);

                        CodecInfo info;
                        info.elementName = gst_plugin_feature_get_name(GST_PLUGIN_FEATURE(factory));

                        gchar *description = gst_pb_utils_get_codec_description(newCaps);
                        info.description = iString::fromUtf8(description);
                        if (description)
                            g_free(description);

                        info.rank = rank;

                        m_codecInfo.insert(std::pair<iString, CodecInfo>(codec, info));
                    }

                    gst_caps_unref(newCaps);
                }
                gst_caps_unref(caps);
            }
        }
    }

    gst_plugin_feature_list_free(elements);
}

#if !GST_CHECK_VERSION(0, 10, 31)
static gboolean element_filter(GstPluginFeature *feature, gpointer user_data)
{
    if (!GST_IS_ELEMENT_FACTORY(feature))
        return FALSE;

    const iGstCodecsInfo::ElementType type = *reinterpret_cast<iGstCodecsInfo::ElementType *>(user_data);

    const gchar *klass = gst_element_factory_get_klass(GST_ELEMENT_FACTORY(feature));
    if (type == iGstCodecsInfo::AudioEncoder && !(strstr(klass, "Encoder") && strstr(klass, "Audio")))
        return FALSE;
    if (type == iGstCodecsInfo::VideoEncoder && !(strstr(klass, "Encoder") && strstr(klass, "Video")))
        return FALSE;
    if (type == iGstCodecsInfo::Muxer && !strstr(klass, "Muxer"))
        return FALSE;

    guint rank = gst_plugin_feature_get_rank(feature);
    if (rank < GST_RANK_MARGINAL)
        return FALSE;

    return TRUE;
}

static gint compare_plugin_func(const void *item1, const void *item2)
{
    GstPluginFeature *f1 = reinterpret_cast<GstPluginFeature *>(const_cast<void *>(item1));
    GstPluginFeature *f2 = reinterpret_cast<GstPluginFeature *>(const_cast<void *>(item2));

    gint diff = gst_plugin_feature_get_rank(f2) - gst_plugin_feature_get_rank(f1);
    if (diff != 0)
        return diff;

    return strcmp(gst_plugin_feature_get_name(f1), gst_plugin_feature_get_name (f2));
}
#endif

GList *iGstCodecsInfo::elementFactories(ElementType elementType) const
{
    #if GST_CHECK_VERSION(0,10,31)
    GstElementFactoryListType gstElementType = 0;
    switch (elementType) {
    case AudioEncoder:
        gstElementType = GST_ELEMENT_FACTORY_TYPE_AUDIO_ENCODER;
        break;
    case VideoEncoder:
        gstElementType = GST_ELEMENT_FACTORY_TYPE_VIDEO_ENCODER;
        break;
    case Muxer:
        gstElementType = GST_ELEMENT_FACTORY_TYPE_MUXER;
        break;
    }

    GList *list = gst_element_factory_list_get_elements(gstElementType, GST_RANK_MARGINAL);
    if (elementType == AudioEncoder) {
        // Manually add "audioconvert" to the list
        // to allow linking with various containers.
        auto factory = gst_element_factory_find("audioconvert");
        if (factory)
            list = g_list_prepend(list, factory);
    }

    return list;
    #else
    GList *result = gst_registry_feature_filter(gst_registry_get_default(),
                                                element_filter,
                                                FALSE, &elementType);
    result = g_list_sort(result, compare_plugin_func);
    return result;
    #endif
}

} // namespace iShell
