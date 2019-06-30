#include "Win8PlusKernelSession.hpp"

#include <evntcons.h>

#include "OS/ETW/ETWSessionCommon.hpp"
#include "Utility/Asserts.hpp"

namespace ETWP {

Win8PlusKernelSession::Win8PlusKernelSession (const std::wstring& name, ULONG kernelFlags):
    m_handle (NULL),
    m_properties (nullptr),
    m_flags (kernelFlags),
    m_name (name),
    m_started (false)
{

}

Win8PlusKernelSession::~Win8PlusKernelSession ()
{
    if (m_started)
        ETWP_VERIFY (StopETWSession (m_handle, m_name, m_properties.get ()));
}

std::wstring Win8PlusKernelSession::GetName () const
{
    return m_name;
}

bool Win8PlusKernelSession::Start ()
{
    if (ETWP_ERROR (m_started))
        return true;

    // On Windows 8 and later, there can be more than one kernel logger, and with custom session names (but an extra
    //   flag, EVENT_TRACE_SYSTEM_LOGGER_MODE has to be present; yeah, not that it took me days to figure that out...)
    m_started = StartRealTimeETWSession (m_name,
                                 EVENT_TRACE_REAL_TIME_MODE | EVENT_TRACE_SYSTEM_LOGGER_MODE,
                                 m_flags,
                                 &m_handle,
                                 &m_properties);

    return m_started;
}

bool Win8PlusKernelSession::Stop ()
{
    if (ETWP_ERROR (!m_started))
        return true;

    bool success = ControlTraceW (m_handle, m_name.c_str (), m_properties.get (), EVENT_TRACE_CONTROL_STOP) ==
        ERROR_SUCCESS;

    m_started = !success;

    return success;
}

TRACEHANDLE Win8PlusKernelSession::GetNativeHandle () const
{
    return m_handle;
}

bool Win8PlusKernelSession::EnableProvider (LPCGUID pProviderID,
                                            bool collectStacks,
                                            UCHAR level /*= TRACE_LEVEL_VERBOSE*/,
                                            ULONGLONG mathcAnyKeyword /*= 0*/,
                                            ULONGLONG mathcAllKeyword /*= 0*/)
{
    ENABLE_TRACE_PARAMETERS traceParams = {};
    traceParams.Version = ENABLE_TRACE_PARAMETERS_VERSION_2;
    traceParams.EnableProperty = collectStacks ? EVENT_ENABLE_PROPERTY_STACK_TRACE : 0;
    traceParams.ControlFlags = 0;
    traceParams.SourceId = *pProviderID;
    traceParams.FilterDescCount = 0;

    return EnableTraceEx2 (m_handle,
                           pProviderID,
                           EVENT_CONTROL_CODE_ENABLE_PROVIDER,
                           level,
                           mathcAnyKeyword,
                           mathcAllKeyword,
                           0,
                           &traceParams) == ERROR_SUCCESS;
}

bool Win8PlusKernelSession::DisableProvider (LPCGUID pProviderID)
{
    ENABLE_TRACE_PARAMETERS traceParams = {};
    traceParams.Version = ENABLE_TRACE_PARAMETERS_VERSION_2;
    traceParams.EnableProperty = 0;
    traceParams.ControlFlags = 0;
    traceParams.SourceId = *pProviderID;
    traceParams.FilterDescCount = 0;

    return EnableTraceEx2 (m_handle,
                           pProviderID,
                           EVENT_CONTROL_CODE_DISABLE_PROVIDER,
                           TRACE_LEVEL_VERBOSE,
                           0,
                           0,
                           0,
                           &traceParams) == ERROR_SUCCESS;
}

ULONG Win8PlusKernelSession::GetKernelFlags () const
{
    return m_flags;
}

}   // namespace ETWP