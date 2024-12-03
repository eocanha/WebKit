/*
 * Copyright (C) 2014 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PlatformScreen.h"

#include "DestinationColorSpace.h"
#include "FloatRect.h"
#include "NotImplemented.h"
#include "Widget.h"
#include "stdio.h"

namespace WebCore {

int screenDepth(Widget*)
{
    notImplemented();
    return 24;
}

int screenDepthPerComponent(Widget*)
{
    notImplemented();
    return 8;
}

bool screenIsMonochrome(Widget*)
{
    notImplemented();
    return false;
}

bool screenHasInvertedColors()
{
    return false;
}

double screenDPI()
{
    notImplemented();
    return 96;
}

void setScreenDPIObserverHandler(Function<void()>&&, void*)
{
    notImplemented();
}

FloatRect screenRect(Widget* widget)
{
    // WPE can't offer any more useful information about the screen size,
    // so we use the Widget's bounds rectangle (size of which equals the WPE view size).

    if (!widget)
        return { };
    return widget->boundsRect();
}

FloatRect screenAvailableRect(Widget* widget)
{
    return screenRect(widget);
}

DestinationColorSpace screenColorSpace(Widget*)
{
    return DestinationColorSpace::SRGB();
}

bool screenSupportsExtendedColor(Widget*)
{
    return false;
}

#if ENABLE(TOUCH_EVENTS)
bool screenHasTouchDevice()
{
    return true;
}

bool screenIsTouchPrimaryInputDevice()
{
    return true;
}
#endif

struct FileHandleDeleter {
    void operator()(FILE* f) { fclose(f); }
};

using FileHandle = std::unique_ptr<FILE, FileHandleDeleter>;

static bool tryOpeningForUnbufferedReading(FileHandle& handle, const char* filePath)
{
    // Check whether the file handle is already valid.
    if (handle)
        return true;

    // Else, try opening it and disable buffering after opening.
    if (auto* f = fopen(filePath, "r")) {
        setbuf(f, nullptr);
        handle.reset(f);
        return true;
    }

    // Could not produce a valid handle.
    return false;
}

bool screenSupportsHighDynamicRange(Widget*)
{
    printf("### %s\n", __PRETTY_FUNCTION__); fflush(stdout);

    static const char* s_procBrcmBhdmEdid = "/proc/brcm/bhdm_edid";

    FileHandle procBrcmBhdmEdidFile;
    if (tryOpeningForUnbufferedReading(procBrcmBhdmEdidFile, s_procBrcmBhdmEdid)
        && procBrcmBhdmEdidFile.get() && !fseek(procBrcmBhdmEdidFile.get(), 0, SEEK_SET)) {
        while (!feof(procBrcmBhdmEdidFile.get())) {
            constexpr unsigned BRCM_BHDM_EDID_LINE_BUFFER_SIZE = 1024;
            char line[BRCM_BHDM_EDID_LINE_BUFFER_SIZE] = { 0 };
            //size_t amount;
            if (fgets(line, BRCM_BHDM_EDID_LINE_BUFFER_SIZE, procBrcmBhdmEdidFile.get()))
                break;
            printf("### %s: --- %s ---\n", __PRETTY_FUNCTION__, line); fflush(stdout);
/*
            if (!strcmp(token, "MemTotal:"))
                memoryTotal = amount;
            else if (!strcmp(token, "MemFree:"))
                memoryFree = amount;
            else if (!strcmp(token, "MemAvailable:"))
                memoryAvailable = amount;
            else if (!strcmp(token, "Active(file):"))
                activeFile = amount;
            else if (!strcmp(token, "Inactive(file):"))
                inactiveFile = amount;
            else if (!strcmp(token, "SReclaimable:"))
                slabReclaimable = amount;

            if (memoryTotal != notSet && memoryFree != notSet && activeFile != notSet && inactiveFile != notSet && slabReclaimable != notSet)
                break;
*/
        }
    }

    return false;
}


} // namespace WebCore
