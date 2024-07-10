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

#pragma once

#if USE(GSTREAMER)

#include "GStreamerCommon.h"
#include "GStreamerQuirks.h"
#include "MediaPlayerPrivateGStreamer.h"
#include <wtf/Vector.h>

namespace WebCore {

class GStreamerQuirkBroadcomBase : public GStreamerQuirk {
public:
    GStreamerQuirkBroadcomBase();

    bool needsBufferingPercentageCorrection() const { return true; }
    bool queryBufferingPercentage(MediaPlayerPrivateGStreamer*, const char*& elementName, GRefPtr<GstQuery>&) const;
    int correctBufferingPercentage(MediaPlayerPrivateGStreamer*, int originalBufferingPercentage, GstBufferingMode) const;
    void resetBufferingPercentage(MediaPlayerPrivateGStreamer*, int bufferingPercentage) const;
    void setupBufferingPercentageCorrection(MediaPlayerPrivateGStreamer*, GstState currentState, GstState newState, GstElement*) const;

private:
    class MovingAverage {
    public:
        MovingAverage(size_t length)
            : m_values(length)
        {
        }

        void reset(int value)
        {
            for (size_t i = 0; i < m_values.size(); i++)
                m_values[i] = value;
        }

        int accumulate(int value)
        {
            intmax_t sum = 0;
            for (size_t i = 1; i < m_values.size(); i++) {
                m_values[i - 1] = m_values[i];
                sum += m_values[i - 1];
            }
            m_values[m_values.size() - 1] = value;
            sum += value;
            intmax_t result = sum / m_values.size();
            ASSERT(result < std::numeric_limits<int>::max());
            return result;
        }
    private:
        Vector<int> m_values;
    };

    using GStreamerQuirkBase::GStreamerQuirkState;

    class GStreamerQuirkBroadcomBaseState : public GStreamerQuirkState {
    public:
        GStreamerQuirkBroadcomBaseState() = delete;
        GStreamerQuirkBroadcomBaseState(const void* owner)
            : GStreamerQuirkState(owner)
        {
        }

        virtual ~GStreamerQuirkBroadcomBaseState() = default;

        GRefPtr<GstElement> m_vidfilter;
        GRefPtr<GstElement> m_multiqueue;
        GRefPtr<GstElement> m_queue2;
        MovingAverage m_streamBufferingLevelMovingAverage { 10 };
    };

    bool isEnsuredOwnedState(MediaPlayerPrivateGStreamer*) const;
};

} // namespace WebCore

#endif // USE(GSTREAMER)
