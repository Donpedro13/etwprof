#ifndef ETWP_WIN_INTERNAL_HPP
#define ETWP_WIN_INTERNAL_HPP

#include <windows.h>
#include <evntrace.h>

// NTSTATUS values
#define STATUS_SUCCESS 0L

namespace ETWP {
namespace WinInternal {	// Undocumented Windows structures etc. used throughout this file

// Thanks to Geoff Chappell (www.geoffchappell.com) for these structs and enums
enum SYSTEM_INFORMATION_CLASS {
    // ...
    SystemPerformanceTraceInformation = 0x1F    // 31
    // ...
};

enum EVENT_TRACE_INFORMATION_CLASS {
    // ...
    EventTraceTimeProfileInformation = 3,
    // ...
    EventTraceStackCachingInformation = 0x10
    // ...
};

struct EVENT_TRACE_TIME_PROFILE_INFORMATION {
    EVENT_TRACE_INFORMATION_CLASS EventTraceInformationClass;
    ULONG ProfileInterval;
};

struct EVENT_TRACE_STACK_CACHING_INFORMATION {
    EVENT_TRACE_INFORMATION_CLASS EventTraceInformationClass;
    TRACEHANDLE TraceHandle;
    BOOLEAN Enabled;
    UCHAR Reserved[3];
    ULONG CacheSize;
    ULONG BucketCount;
};

NTSTATUS NtQuerySystemInformation (SYSTEM_INFORMATION_CLASS SystemInformationClass,
                                   PVOID SystemInformation,
                                   ULONG SystemInformationLength,
                                   ULONG* ReturnLength);
NTSTATUS NtSetSystemInformation (SYSTEM_INFORMATION_CLASS SystemInformationClass,
                                 PVOID SystemInformation,
                                 ULONG SystemInformationLength);

}   // namespace WinInternal
}   // namespace ETWP

#endif  // #ifndef ETWP_WIN_INTERNAL_HPP