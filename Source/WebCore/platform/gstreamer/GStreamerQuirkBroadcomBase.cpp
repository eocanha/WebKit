/*
 * Copyright (C) 2024 Igalia S.L
 * Copyright (C) 2024 Metrological Group B.V.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * aint with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "config.h"
#include "GStreamerQuirkBroadcom.h"

#if USE(GSTREAMER)

#include "GStreamerCommon.h"

namespace WebCore {

GST_DEBUG_CATEGORY_STATIC(webkit_broadcom_base_quirks_debug);
#define GST_CAT_DEFAULT webkit_broadcom_base_quirks_debug

GStreamerQuirkBroadcomBase::GStreamerQuirkBroadcomBase()
{
    GST_DEBUG_CATEGORY_INIT(webkit_broadcom_base_quirks_debug, "webkitquirksbroadcombase", 0, "WebKit Broadcom Base Quirks");
}

bool GStreamerQuirkBroadcomBase::queryBufferingPercentage(MediaPlayerPrivateGStreamer* playerPrivate, const char*& elementName, GRefPtr<GstQuery>& query) const
{
    bool shouldDownload = playerPrivate->m_fillTimer.isActive();
    if (shouldDownload || !playerPrivate->m_queue2)
        return false;
    if (gst_element_query(playerPrivate->m_queue2.get(), query.get())) {
        elementName = "queue2";
        return true;
    }
    return false;
}

int GStreamerQuirkBroadcomBase::correctBufferingPercentage(MediaPlayerPrivateGStreamer* playerPrivate, int originalBufferingPercentage, GstBufferingMode mode) const
{
    // The Nexus playpump buffers a lot of data. Let's add it as if it had been buffered by the GstQueue2
    // (only when using in-memory buffering), so we get more realistic percentages.
    if (mode != GST_BUFFERING_STREAM || !playerPrivate->m_vidfilter)
        return originalBufferingPercentage;

    // The Nexus playpump buffers a lot of data. Let's add it as if it had been buffered by the GstQueue2
    // (only when using in-memory buffering), so we get more realistic percentages.
    int correctedBufferingPercentage1 = originalBufferingPercentage;
    int correctedBufferingPercentage2 = originalBufferingPercentage;
    unsigned int maxSizeBytes = 0;

    // We don't trust the buffering percentage when it's 0, better rely on current-level-bytes and compute a new buffer level accordingly.
    g_object_get(playerPrivate->m_queue2.get(), "max-size-bytes", &maxSizeBytes, nullptr);
    if (!originalBufferingPercentage && playerPrivate->m_queue2) {
        unsigned int currentLevelBytes = 0;
        g_object_get(playerPrivate->m_queue2.get(), "current-level-bytes", &currentLevelBytes, nullptr);
        correctedBufferingPercentage1 = currentLevelBytes > maxSizeBytes ? 100 : static_cast<int>(currentLevelBytes * 100 / maxSizeBytes);
    }

    unsigned int playpumpBufferedBytes = 0;
    if (playerPrivate->m_vidfilter)
        g_object_get(GST_OBJECT(playerPrivate->m_vidfilter.get()), "buffered-bytes", &playpumpBufferedBytes, nullptr);

    unsigned int multiqueueBufferedBytes = 0;
    if (playerPrivate->m_multiqueue) {
        GUniqueOutPtr<GstStructure> stats;
        g_object_get(playerPrivate->m_multiqueue.get(), "stats", &stats.outPtr(), nullptr);
        const GValue* queues = gst_structure_get_value(stats.get(), "queues");
        unsigned int size = gst_value_array_get_size(queues);
        for (unsigned int i = 0; i < size; i++) {
            unsigned int bytes = 0;
            if (gst_structure_get_uint(gst_value_get_structure(gst_value_array_get_value(queues, i)), "bytes", &bytes))
                multiqueueBufferedBytes += bytes;
        }
    }

    // Current-level-bytes seems to be inacurate, so we compute its value from the buffering percentage.
    size_t currentLevelBytes = static_cast<size_t>(maxSizeBytes) * static_cast<size_t>(originalBufferingPercentage) / static_cast<size_t>(100)
        + static_cast<size_t>(playpumpBufferedBytes) + static_cast<size_t>(multiqueueBufferedBytes);
    correctedBufferingPercentage2 = currentLevelBytes > maxSizeBytes ? 100 : static_cast<int>(currentLevelBytes * 100 / maxSizeBytes);

    if (correctedBufferingPercentage2 >= 100)
        playerPrivate->m_streamBufferingLevelMovingAverage.reset(100);
    int averagedBufferingPercentage = playerPrivate->m_streamBufferingLevelMovingAverage.accumulate(correctedBufferingPercentage2);

#ifndef GST_DISABLE_GST_DEBUG
    const char* extraElements = playerPrivate->m_multiqueue ? "playpump and multiqueue" : "playpump";
    if (!originalBufferingPercentage) {
        GST_DEBUG("[Buffering] Buffering: mode: GST_BUFFERING_STREAM, status: %d%% (corrected to %d%% with current-level-bytes, "
            "to %d%% with %s content, and to %d%% with moving average).", originalBufferingPercentage, correctedBufferingPercentage1,
            correctedBufferingPercentage2, extraElements, averagedBufferingPercentage);
    } else {
        GST_DEBUG("[Buffering] Buffering: mode: GST_BUFFERING_STREAM, status: %d%% (corrected to %d%% with %s content and "
            "to %d%% with moving average).", originalBufferingPercentage, correctedBufferingPercentage2, extraElements,
            averagedBufferingPercentage);
    }
#endif

    return averagedBufferingPercentage;
}

void GStreamerQuirkBroadcomBase::setupBufferingPercentageCorrection(MediaPlayerPrivateGStreamer* playerPrivate, GstState currentState, GstState newState, GstElement* element) const
{
    if (currentState == GST_STATE_NULL && newState == GST_STATE_READY
        && g_strstr_len(GST_ELEMENT_NAME(element), 13, "brcmvidfilter")) {
        playerPrivate->m_vidfilter = element;

        // Also get the multiqueue (if there's one) attached to the vidfilter. We'll need it later to correct the buffering level.
        for (auto* sinkPad : GstIteratorAdaptor<GstPad>(GUniquePtr<GstIterator>(gst_element_iterate_sink_pads(playerPrivate->m_vidfilter.get())))) {
            GRefPtr<GstPad> peerSrcPad = adoptGRef(gst_pad_get_peer(sinkPad));
            if (peerSrcPad) {
                GRefPtr<GstElement> peerElement = adoptGRef(GST_ELEMENT(gst_pad_get_parent(peerSrcPad.get())));
                // The multiqueue reference is useless if we can't access its stats (on older GStreamer versions).
                if (peerElement && g_strstr_len(GST_ELEMENT_NAME(peerElement.get()), 10, "multiqueue")
                    && g_object_class_find_property(G_OBJECT_GET_CLASS(peerElement.get()), "stats"))
                    playerPrivate->m_multiqueue = peerElement;
            }
            break;
        }
    } else if (currentState == GST_STATE_NULL && newState == GST_STATE_READY
        && g_strstr_len(GST_ELEMENT_NAME(element), 6, "queue2")) {
        playerPrivate->m_queue2 = element;
    } else if (currentState == GST_STATE_READY && newState == GST_STATE_NULL
        && g_strstr_len(GST_ELEMENT_NAME(element), 13, "brcmvidfilter")) {
        playerPrivate->m_vidfilter = nullptr;
        playerPrivate->m_multiqueue = nullptr;
    } else if (currentState == GST_STATE_READY && newState == GST_STATE_NULL
        && g_strstr_len(GST_ELEMENT_NAME(element), 6, "queue2")) {
        playerPrivate->m_queue2 = nullptr;
    }
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER)
