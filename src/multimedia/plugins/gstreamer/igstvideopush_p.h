/////////////////////////////////////////////////////////////////
/// Copyright 2018-2020
/// All rights reserved.
/////////////////////////////////////////////////////////////////
/// @file    igstvideopush_p.h
/// @brief   shared appsrc plumbing for the frame-driven video classes
/// @version 1.0
/// @author  ncjiakechong@gmail.com
/////////////////////////////////////////////////////////////////
#ifndef IGSTVIDEOPUSH_P_H
#define IGSTVIDEOPUSH_P_H

#include <gst/gst.h>
#include <gst/app/gstappsrc.h>

#include <core/kernel/iobject.h>
#include <core/thread/iatomiccounter.h>
#include <core/utils/isize.h>
#include <core/utils/istring.h>

#include <multimedia/video/ivideoframe.h>
#include <multimedia/codec/ivideocodecsettings.h>
#include <multimedia/imultimedia.h>

namespace iShell {

class iGstVideoInput : public iObject
{
    IX_OBJECT(iGstVideoInput)
public:
    explicit iGstVideoInput(GstAppSrc* appsrc, iObject* parent = IX_NULLPTR);
    ~iGstVideoInput();
    iMultimedia::SubmitResult checkCapacity(guint64 bytes);
    iMultimedia::SubmitResult checkFrame(const iVideoFrame& frame);
    void setCapacityAvailable(bool available);
    void stop();
    void readyToSubmit();

private:
    static void needData(GstAppSrc*, guint, gpointer data);
    void scheduleReady();
    void checkReady();

    GstAppSrc* m_appsrc;
    iAtomicCounter<int> m_waiting;
    iAtomicCounter<int> m_notificationPending;
    guint64 m_requestedBytes;
    bool m_capacityAvailable;
    bool m_enabled;
    IX_DISABLE_COPY(iGstVideoInput)
};

bool gstOnOwnerThread(const iObject* object);
bool gstAppsrcCanAccept(GstAppSrc* appsrc, guint64 bytes);
iString gstErrorMessage(GstMessage* message);
bool gstPipelineError(GstElement* pipeline, GstMessage* message, iString* error);
GstClockTime gstSampleRunningTime(GstSample* sample, GstClockTime timestamp);
xint64 gstSampleTimeUs(GstSample* sample, GstClockTime timestamp);
bool gstFrameTiming(const iVideoFrame& frame, iVideoTimestampPolicy policy,
                    GstClockTime period, GstClockTime nextPts, GstClockTime lastPts,
                    GstClockTime* pts, GstClockTime* duration);

/// Retains or copies a Format_BGR24 frame, stamping explicit timestamps.
/// Callers stamp their own PTS because the frame id mapping rides on it.
iMultimedia::SubmitResult gstPushBgrFrame(GstAppSrc* appsrc, const iVideoFrame& frame,
                     GstClockTime pts, GstClockTime duration);

/// Resolves the pipeline and its named appsrc, leaving it in PLAYING.
/// Returns false and fills @p error without leaking a partial pipeline.
bool gstBuildPushPipeline(const iString& description, const char* srcName,
                          GstElement** pipeline, GstAppSrc** appsrc,
                          iString* error);

} // namespace iShell

#endif // IGSTVIDEOPUSH_P_H
