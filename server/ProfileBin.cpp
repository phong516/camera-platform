#include "ProfileBin.hpp"

ProfileBin::ProfileBin(ProfileInfo profile): m_profile(profile)
{
}

ProfileBin::~ProfileBin()
{
    if (m_bin)
    {
        gst_object_unref(m_bin);
        m_valve = nullptr;
        m_appSink = nullptr;
    }
    if (m_valve)
    {
        gst_object_unref(m_valve);
    }
    if (m_appSink)
    {
        gst_object_unref(m_appSink);
    }
}

GstElement *ProfileBin::createElement()
{
    m_bin = gst_bin_new("profile-bin");
    m_valve = gst_element_factory_make("valve", "valve");
    m_appSink = gst_element_factory_make("appsink", "app-sink");

    GstElement *queue = gst_element_factory_make("queue", "queue");
    GstElement *scale = gst_element_factory_make("videoscale", "scale");
    GstElement *capsFilter = gst_element_factory_make("capsfilter", "caps-filter");
    GstElement *enc = gst_element_factory_make("x264enc", "enc");

    if (!m_bin || !m_valve || !m_appSink || !queue || !scale || !capsFilter || !enc)
    {
        g_clear_object(&m_bin);
        g_clear_object(&m_valve);
        g_clear_object(&m_appSink);
        return nullptr;
    }

    GstCaps *caps = gst_caps_new_simple("video/x-raw",
                                        "format", G_TYPE_STRING, "I420",
                                        "width", G_TYPE_INT, m_profile.width,
                                        "height", G_TYPE_INT, m_profile.height,
                                        "framerate", GST_TYPE_FRACTION, m_profile.fps, 1,
                                        nullptr);
    g_object_set(capsFilter, "caps", caps, NULL);
    gst_caps_unref(caps);

    gst_util_set_object_arg(G_OBJECT(enc), "speed-preset", "ultrafast");
    gst_util_set_object_arg(G_OBJECT(enc), "tune", "zerolatency");
    g_object_set(enc, "key-int-max", m_profile.fps, "bitrate", m_profile.bitrateKbps, NULL);

    setDrop(false);

    gst_bin_add_many(GST_BIN(m_bin), queue, m_valve, scale, capsFilter, enc, m_appSink, NULL);
    if (!gst_element_link_many(queue, m_valve, scale, capsFilter, enc, m_appSink, NULL))
    {
        g_clear_object(&m_bin);
        m_valve = nullptr;
        m_appSink = nullptr;
        return nullptr;
    }

    GstPad *sinkPad = gst_element_get_static_pad(queue, "sink");
    GstPad *ghostPad = gst_ghost_pad_new("sink", sinkPad);
    gst_element_add_pad(m_bin, ghostPad);
    gst_object_unref(sinkPad);
    gst_element_no_more_pads(m_bin);

    return m_bin;
}

GstElement *ProfileBin::element() const
{
    return m_bin;
}

GstElement *ProfileBin::appSink() const
{
    return m_appSink;
}

void ProfileBin::setDrop(bool drop)
{
    if (m_valve)
    {
        g_object_set(m_valve, "drop", drop, NULL);
    }
}

bool ProfileBin::isDropped() const
{
    gboolean drop = false;
    if (m_valve)
    {
        g_object_get(m_valve, "drop", &drop, NULL);
    }
    return drop;
}

const ProfileInfo &ProfileBin::profile() const
{
    return m_profile;
}

