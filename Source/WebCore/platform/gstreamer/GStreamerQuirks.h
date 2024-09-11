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
#include "MediaPlayer.h"
#include <wtf/Forward.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMalloc.h>
#include <wtf/Vector.h>

namespace WebCore {

class MediaPlayerPrivateGStreamer;

enum class ElementRuntimeCharacteristics : uint8_t {
    IsMediaStream = 1 << 0,
    HasVideo = 1 << 1,
    HasAudio = 1 << 2,
    IsLiveStream = 1 << 3,
};

class GStreamerQuirkBase {
    WTF_MAKE_TZONE_ALLOCATED(GStreamerQuirkBase);

public:
    GStreamerQuirkBase() = default;
    virtual ~GStreamerQuirkBase() = default;

    virtual const char* identifier() const = 0;

    // Interface of classes supplied to MediaPlayerPrivateGStreamer to store values that the quirks will need for their job.
    class GStreamerQuirkState {
    public:
        GStreamerQuirkState()
            : m_owner(nullptr)
        {
        }

        GStreamerQuirkState(const void* owner)
            : m_owner(owner)
        {
        }

        virtual ~GStreamerQuirkState() = default;
        bool isOwnedBy(const void* owner) { return m_owner == owner; }
        bool isOwned() { return m_owner; }

    private:
        // For identification purposes only, to avoid the wrong GStreamerQuirkBase subclass to use a state that
        // doesn't belong to itself. The pointer shouldn't be used to access the instance.
        const void* m_owner = nullptr;
    };
};

class GStreamerQuirk : public GStreamerQuirkBase {
    WTF_MAKE_TZONE_ALLOCATED(GStreamerQuirk);
public:
    GStreamerQuirk() = default;
    virtual ~GStreamerQuirk() = default;

    virtual bool isPlatformSupported() const { return true; }
    virtual GstElement* createAudioSink() { return nullptr; }
    virtual GstElement* createWebAudioSink() { return nullptr; }
    virtual void configureElement(GstElement*, const OptionSet<ElementRuntimeCharacteristics>&) { }
    virtual std::optional<bool> isHardwareAccelerated(GstElementFactory*) { return std::nullopt; }
    virtual std::optional<GstElementFactoryListType> audioVideoDecoderFactoryListType() const { return std::nullopt; }
    virtual Vector<String> disallowedWebAudioDecoders() const { return { }; }
    virtual unsigned getAdditionalPlaybinFlags() const { return 0; }
    virtual bool shouldParseIncomingLibWebRTCBitStream() const { return true; }
    virtual bool needsBufferingPercentageCorrection() const { return false; }
    virtual bool queryBufferingPercentage(MediaPlayerPrivateGStreamer*, const char*&, GRefPtr<GstQuery>&) const { return false; }
    virtual int correctBufferingPercentage(MediaPlayerPrivateGStreamer*, int originalBufferingPercentage, GstBufferingMode) const { return originalBufferingPercentage; }
    virtual void resetBufferingPercentage(MediaPlayerPrivateGStreamer*, int) const { };
    virtual void setupBufferingPercentageCorrection(MediaPlayerPrivateGStreamer*, GstState, GstState, GstElement*) const { }
};

class GStreamerHolePunchQuirk : public GStreamerQuirkBase {
    WTF_MAKE_TZONE_ALLOCATED(GStreamerHolePunchQuirk);
public:
    GStreamerHolePunchQuirk() = default;
    virtual ~GStreamerHolePunchQuirk() = default;

    virtual GstElement* createHolePunchVideoSink(bool, const MediaPlayer*) { return nullptr; }
    virtual bool setHolePunchVideoRectangle(GstElement*, const IntRect&) { return false; }
    virtual bool requiresClockSynchronization() const { return true; }
};

class GStreamerQuirksManager : public RefCounted<GStreamerQuirksManager> {
    friend NeverDestroyed<GStreamerQuirksManager>;
    WTF_MAKE_TZONE_ALLOCATED(GStreamerQuirksManager);

public:
    static GStreamerQuirksManager& singleton();

    static RefPtr<GStreamerQuirksManager> createForTesting()
    {
        return adoptRef(*new GStreamerQuirksManager(true, false));
    }

    bool isEnabled() const;

    GstElement* createAudioSink();
    GstElement* createWebAudioSink();
    void configureElement(GstElement*, OptionSet<ElementRuntimeCharacteristics>&&);
    std::optional<bool> isHardwareAccelerated(GstElementFactory*) const;
    GstElementFactoryListType audioVideoDecoderFactoryListType() const;
    Vector<String> disallowedWebAudioDecoders() const;

    bool supportsVideoHolePunchRendering() const;
    GstElement* createHolePunchVideoSink(bool isLegacyPlaybin, const MediaPlayer*);
    void setHolePunchVideoRectangle(GstElement*, const IntRect&);
    bool sinksRequireClockSynchronization() const;

    void setHolePunchEnabledForTesting(bool);

    unsigned getAdditionalPlaybinFlags() const;

    bool shouldParseIncomingLibWebRTCBitStream() const;

    bool needsBufferingPercentageCorrection() const;
    bool queryBufferingPercentage(MediaPlayerPrivateGStreamer*, const char*& elementName, GRefPtr<GstQuery>&) const;
    int correctBufferingPercentage(MediaPlayerPrivateGStreamer*, int originalBufferingPercentage, GstBufferingMode) const;
    void resetBufferingPercentage(MediaPlayerPrivateGStreamer*, int bufferingPercentage) const;
    void setupBufferingPercentageCorrection(MediaPlayerPrivateGStreamer*, GstState currentState, GstState newState, GstElement*) const;

private:
    GStreamerQuirksManager(bool, bool);

    Vector<std::unique_ptr<GStreamerQuirk>> m_quirks;
    std::unique_ptr<GStreamerHolePunchQuirk> m_holePunchQuirk;
    bool m_isForTesting { false };
};

} // namespace WebCore

#endif // USE(GSTREAMER)
