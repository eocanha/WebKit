/*
 * DebugBreakpoint.h
 *
 *  Created on: Sep 23, 2014
 *      Author: enrique
 */

#ifndef DEBUGBREAKPOINT_H_
#define DEBUGBREAKPOINT_H_

#include "ScriptWrappable.h"
#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/text/WTFString.h>

namespace WebCore
{

class DebugBreakpoint : public ThreadSafeRefCounted<DebugBreakpoint>
{
    WTF_MAKE_FAST_ALLOCATED;
public:
    static RefPtr<DebugBreakpoint> create();

    DebugBreakpoint();
    virtual ~DebugBreakpoint();
    static void printMessage(const String& message);
    static void print(const String& message) { printMessage(message); }
    static void log(const String& message) { printMessage(message); }
    static void crash(const String& message);
};

} /* namespace WebCore */

#endif /* DEBUGBREAKPOINT_H_ */
