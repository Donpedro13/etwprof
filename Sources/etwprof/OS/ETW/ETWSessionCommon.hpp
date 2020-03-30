#ifndef ETWP_ETW_SESSION_COMMON_HPP
#define ETWP_ETW_SESSION_COMMON_HPP

#include <windows.h>
#include <evntrace.h>

#include <memory>
#include <string>

namespace ETWP {

bool StopETWSession (TRACEHANDLE hSession, const std::wstring& name, PEVENT_TRACE_PROPERTIES pProperties);
bool StartRealTimeETWSession (const std::wstring& name,
                              ULONG logFileMode,
                              ULONG enableFlags,
                              PTRACEHANDLE pHandle,
                              std::unique_ptr<EVENT_TRACE_PROPERTIES>* pPropertiesOut);

bool EnableStackCachingForSession (TRACEHANDLE pSession, uint32_t cacheSize, uint32_t bucketCount);

}   // namespace ETWP

#endif  // #ifndef ETWP_ETW_SESSION_COMMON_HPP