/*
 * DebugBreakpoint.cpp
 *
 *  Created on: Sep 23, 2014
 *      Author: enrique
 */

#include "config.h"
#include "DebugBreakpoint.h"
#include <stdio.h>
#include <wtf/text/CString.h>

namespace WebCore
{

RefPtr<DebugBreakpoint> DebugBreakpoint::create()
{
    return adoptRef(new DebugBreakpoint());
}

DebugBreakpoint::DebugBreakpoint()
{
}

DebugBreakpoint::~DebugBreakpoint()
{
}

void DebugBreakpoint::printMessage(const String& message)
{
    printf("%s : %s\n", __PRETTY_FUNCTION__, message.utf8().data());
    fflush(stdout);
}

void DebugBreakpoint::crash(const String& message)
{
    if (message.isEmpty()) {
        printf("%s : EME - crashing\n", __PRETTY_FUNCTION__);
    } else {
        printf("%s : EME - crashing - %s\n", __PRETTY_FUNCTION__, message.utf8().data());
    }
    fflush(stdout);
#if PLATFORM(MAC)
    ASSERT_NOT_REACHED();
#else
    CRASH();
#endif
    return;
}

} /* namespace WebCore */
