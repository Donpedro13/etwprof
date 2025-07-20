#include "CombinedETWSession.hpp"

#include <evntcons.h>

#include "OS/ETW/ETWSessionCommon.hpp"
#include "Utility/Asserts.hpp"

namespace ETWP {

CombinedETWSession::CombinedETWSession (const std::wstring& name, ULONG kernelFlags):
    m_handle (NULL),
    m_properties (nullptr),
    m_flags (kernelFlags),
    m_name (name),
    m_started (false)
{

}

CombinedETWSession::~CombinedETWSession ()
{
    if (m_started)
        StopETWSession (m_handle, m_name, m_properties.get ());
}

std::wstring CombinedETWSession::GetName () const
{
    return m_name;
}

bool CombinedETWSession::Start ()
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

bool CombinedETWSession::Stop ()
{
    if (ETWP_ERROR (!m_started))
        return true;

    m_started = !StopETWSession (m_handle, m_name, m_properties.get ());

    return !m_started;
}

TRACEHANDLE CombinedETWSession::GetNativeHandle () const
{
    return m_handle;
}

bool CombinedETWSession::EnableProvider (LPCGUID pProviderID,
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

bool CombinedETWSession::DisableProvider (LPCGUID pProviderID)
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

ULONG CombinedETWSession::GetKernelFlags () const
{
    return m_flags;
}

}   // namespace ETWP