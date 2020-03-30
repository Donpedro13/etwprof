#include "ETWSessionCommon.hpp"

#include "OS/Utility/WinInternal.hpp"

namespace ETWP {

bool StopETWSession (TRACEHANDLE hSession, const std::wstring& name, PEVENT_TRACE_PROPERTIES pProperties)
{
    return ControlTraceW (hSession, name.c_str (), pProperties, EVENT_TRACE_CONTROL_STOP) == ERROR_SUCCESS;
}

bool StartRealTimeETWSession (const std::wstring& name,
                              ULONG logFileMode,
                              ULONG enableFlags,
                              PTRACEHANDLE pHandle,
                              std::unique_ptr<EVENT_TRACE_PROPERTIES>* pPropertiesOut)
{
    std::unique_ptr<EVENT_TRACE_PROPERTIES> pProperties;
    size_t bufferSize = sizeof (EVENT_TRACE_PROPERTIES) + name.length () * sizeof (wchar_t) + sizeof L'\0';
    pProperties.reset (reinterpret_cast <PEVENT_TRACE_PROPERTIES> (new char[bufferSize]));

    ZeroMemory (pProperties.get (), bufferSize);

    pProperties->Wnode.BufferSize = static_cast<ULONG> (bufferSize);

    pProperties->Wnode.ClientContext = 1;   // Use the default (QueryPerformanceCounter)
    pProperties->Wnode.Flags = WNODE_FLAG_TRACED_GUID;

    pProperties->LogFileMode = logFileMode;
    pProperties->LoggerNameOffset = sizeof (EVENT_TRACE_PROPERTIES);
    pProperties->LogFileNameOffset = 0;

    pProperties->EnableFlags = enableFlags;

    if (StartTraceW (pHandle, name.c_str (), pProperties.get ()) == ERROR_SUCCESS) {
        *pPropertiesOut = std::move (pProperties);

        return true;
    } else {
        return false;
    }
}

bool EnableStackCachingForSession (TRACEHANDLE pSession, uint32_t cacheSize, uint32_t bucketCount)
{
	WinInternal::EVENT_TRACE_STACK_CACHING_INFORMATION stackCacheInfo;
	stackCacheInfo.EventTraceInformationClass = WinInternal::EventTraceStackCachingInformation;
	stackCacheInfo.TraceHandle = pSession;
	stackCacheInfo.Enabled = TRUE;
	stackCacheInfo.CacheSize = cacheSize;
	stackCacheInfo.BucketCount = bucketCount;

	return WinInternal::NtSetSystemInformation (WinInternal::SystemPerformanceTraceInformation,
												&stackCacheInfo,
												sizeof stackCacheInfo) == STATUS_SUCCESS;
}

}   // namespace ETWP