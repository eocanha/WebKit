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

#include <string.h>

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
    if (!isEnsuredOwnedState(playerPrivate))
        return false;
    GStreamerQuirkBroadcomBaseState& state = static_cast<GStreamerQuirkBroadcomBaseState&>(playerPrivate->quirkState());

    if (playerPrivate->shouldDownload() || !state.m_queue2)
        return false;
    if (gst_element_query(state.m_queue2.get(), query.get())) {
        elementName = "queue2";
        return true;
    }
    return false;
}

int GStreamerQuirkBroadcomBase::correctBufferingPercentage(MediaPlayerPrivateGStreamer* playerPrivate, int originalBufferingPercentage, GstBufferingMode mode) const
{
    if (!isEnsuredOwnedState(playerPrivate))
        return originalBufferingPercentage;

    GStreamerQuirkBroadcomBaseState& state = static_cast<GStreamerQuirkBroadcomBaseState&>(playerPrivate->quirkState());

    // The Nexus playpump buffers a lot of data. Let's add it as if it had been buffered by the GstQueue2
    // (only when using in-memory buffering), so we get more realistic percentages.
    if (mode != GST_BUFFERING_STREAM || !state.m_vidfilter)
        return originalBufferingPercentage;

    // The Nexus playpump buffers a lot of data. Let's add it as if it had been buffered by the GstQueue2
    // (only when using in-memory buffering), so we get more realistic percentages.
    int correctedBufferingPercentage1 = originalBufferingPercentage;
    int correctedBufferingPercentage2 = originalBufferingPercentage;
    unsigned maxSizeBytes = 0;

    // We don't trust the buffering percentage when it's 0, better rely on current-level-bytes and compute a new buffer level accordingly.
    g_object_get(state.m_queue2.get(), "max-size-bytes", &maxSizeBytes, nullptr);
    if (!originalBufferingPercentage && state.m_queue2) {
        unsigned currentLevelBytes = 0;
        g_object_get(state.m_queue2.get(), "current-level-bytes", &currentLevelBytes, nullptr);
        correctedBufferingPercentage1 = currentLevelBytes > maxSizeBytes ? 100 : static_cast<int>(currentLevelBytes * 100 / maxSizeBytes);
    }

    unsigned playpumpBufferedBytes = 0;
    if (state.m_vidfilter)
        g_object_get(GST_OBJECT(state.m_vidfilter.get()), "buffered-bytes", &playpumpBufferedBytes, nullptr);

    unsigned multiqueueBufferedBytes = 0;
    if (state.m_multiqueue) {
        GUniqueOutPtr<GstStructure> stats;
        g_object_get(state.m_multiqueue.get(), "stats", &stats.outPtr(), nullptr);
        const GValue* queues = gst_structure_get_value(stats.get(), "queues");
        unsigned size = gst_value_array_get_size(queues);
        for (unsigned i = 0; i < size; i++) {
            unsigned bytes = 0;
            if (gst_structure_get_uint(gst_value_get_structure(gst_value_array_get_value(queues, i)), "bytes", &bytes))
                multiqueueBufferedBytes += bytes;
        }
    }

    // Current-level-bytes seems to be inacurate, so we compute its value from the buffering percentage.
    size_t currentLevelBytes = static_cast<size_t>(maxSizeBytes) * static_cast<size_t>(originalBufferingPercentage) / static_cast<size_t>(100)
        + static_cast<size_t>(playpumpBufferedBytes) + static_cast<size_t>(multiqueueBufferedBytes);
    correctedBufferingPercentage2 = currentLevelBytes > maxSizeBytes ? 100 : static_cast<int>(currentLevelBytes * 100 / maxSizeBytes);

    if (correctedBufferingPercentage2 >= 100)
        state.m_streamBufferingLevelMovingAverage.reset(100);
    int averagedBufferingPercentage = state.m_streamBufferingLevelMovingAverage.accumulate(correctedBufferingPercentage2);

#ifndef GST_DISABLE_GST_DEBUG
    const char* extraElements = state.m_multiqueue ? "playpump and multiqueue" : "playpump";
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

void GStreamerQuirkBroadcomBase::resetBufferingPercentage(MediaPlayerPrivateGStreamer* playerPrivate, int bufferingPercentage) const
{
    if (!isEnsuredOwnedState(playerPrivate))
        return;

    GStreamerQuirkBroadcomBaseState& state = static_cast<GStreamerQuirkBroadcomBaseState&>(playerPrivate->quirkState());

    state.m_streamBufferingLevelMovingAverage.reset(bufferingPercentage);
}

void GStreamerQuirkBroadcomBase::setupBufferingPercentageCorrection(MediaPlayerPrivateGStreamer* playerPrivate, GstState currentState, GstState newState, GstElement* element) const
{
    if (!isEnsuredOwnedState(playerPrivate))
        return;

    GStreamerQuirkBroadcomBaseState& state = static_cast<GStreamerQuirkBroadcomBaseState&>(playerPrivate->quirkState());

    // This code must support being run from different GStreamerQuirkBroadcomBase subclasses without breaking. Only the
    // first subclass instance should run.
    if (currentState == GST_STATE_NULL && newState == GST_STATE_READY
        && g_strstr_len(GST_ELEMENT_NAME(element), 13, "brcmvidfilter")) {
        state.m_vidfilter = element;

        // Also get the multiqueue (if there's one) attached to the vidfilter. We'll need it later to correct the buffering level.
        for (auto* sinkPad : GstIteratorAdaptor<GstPad>(GUniquePtr<GstIterator>(gst_element_iterate_sink_pads(state.m_vidfilter.get())))) {
            GRefPtr<GstPad> peerSrcPad = adoptGRef(gst_pad_get_peer(sinkPad));
            if (peerSrcPad) {
                GRefPtr<GstElement> peerElement = adoptGRef(GST_ELEMENT(gst_pad_get_parent(peerSrcPad.get())));
                // The multiqueue reference is useless if we can't access its stats (on older GStreamer versions).
                if (peerElement && g_strstr_len(GST_ELEMENT_NAME(peerElement.get()), 10, "multiqueue")
                    && g_object_class_find_property(G_OBJECT_GET_CLASS(peerElement.get()), "stats"))
                    state.m_multiqueue = peerElement;
            }
            break;
        }
    } else if (currentState == GST_STATE_NULL && newState == GST_STATE_READY
        && g_strstr_len(GST_ELEMENT_NAME(element), 6, "queue2")) {
        state.m_queue2 = element;
    } else if (currentState == GST_STATE_READY && newState == GST_STATE_NULL
        && g_strstr_len(GST_ELEMENT_NAME(element), 13, "brcmvidfilter")) {
        state.m_vidfilter = nullptr;
        state.m_multiqueue = nullptr;
    } else if (currentState == GST_STATE_READY && newState == GST_STATE_NULL
        && g_strstr_len(GST_ELEMENT_NAME(element), 6, "queue2")) {
        state.m_queue2 = nullptr;
    }
}

bool GStreamerQuirkBroadcomBase::isEnsuredOwnedState(MediaPlayerPrivateGStreamer* playerPrivate) const
{
    // The first GStreamerQuirk subclass that needs a GStreamerQuirkState will initialize it with its own class,
    // replacing the original GStreamerQuirkState abstract placeholder. This means that only one subclass will own
    // and use the state. The other subclasses will bail out, because the concrete state doesn't belong to them.
    if (!playerPrivate->quirkState().isOwned()) {
        GST_DEBUG("%s %p setting up quirk state on MediaPlayerPrivate %p", identifier(), this, playerPrivate);
        playerPrivate->setQuirkState(GStreamerQuirkBroadcomBaseState(reinterpret_cast<const void*>(this)));
    }

    // But if it has been initialized before by a different subclass, we bail out.
    if (!playerPrivate->quirkState().isOwnedBy(this))
        return false;

    return true;
}

#undef GST_CAT_DEFAULT

} // namespace WebCore

#endif // USE(GSTREAMER)
