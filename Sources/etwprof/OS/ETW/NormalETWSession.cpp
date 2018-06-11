#include "NormalETWSession.hpp"

#include "OS/ETW/ETWSessionCommon.hpp"
#include "Utility/Asserts.hpp"

namespace ETWP {

NormalETWSession::NormalETWSession (const std::wstring& name):
    m_handle (NULL),
    m_name (name),
    m_properties (nullptr),
    m_started (false)
{

}

NormalETWSession::~NormalETWSession ()
{
    if (m_started)
        ETWP_VERIFY (StopETWSession (m_handle, m_name, m_properties.get ()));
}

std::wstring NormalETWSession::GetName () const
{
    return m_name;
}

bool NormalETWSession::Start ()
{
    if (ETWP_ERROR (m_started))
        return true;

    m_started = StartETWSession (m_name,
                                 EVENT_TRACE_REAL_TIME_MODE,
                                 0,
                                 &m_handle,
                                 &m_properties);

    return m_started;
}

bool NormalETWSession::Stop ()
{
    if (ETWP_ERROR (!m_started))
        return true;

    bool success = ControlTraceW (m_handle, m_name.c_str (), m_properties.get (), EVENT_TRACE_CONTROL_STOP) ==
        ERROR_SUCCESS;

    m_started = !success;

    return success;
}

TRACEHANDLE NormalETWSession::GetNativeHandle () const
{
    return m_handle;
}

bool NormalETWSession::EnableProvider (LPCGUID pProviderID,
                                       UCHAR level /*= TRACE_LEVEL_VERBOSE*/,
                                       ULONGLONG mathcAnyKeyword /*= 0*/,
                                       ULONGLONG mathcAllKeyword /*= 0*/)
{
    ENABLE_TRACE_PARAMETERS traceParams = {};
    traceParams.Version = ENABLE_TRACE_PARAMETERS_VERSION_2;
    traceParams.EnableProperty = 0;
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

bool NormalETWSession::DisableProvider (LPCGUID pProviderID)
{
    ENABLE_TRACE_PARAMETERS traceParams = {};
    traceParams.Version = ENABLE_TRACE_PARAMETERS_VERSION_2;
    traceParams.EnableProperty = 0;
    traceParams.ControlFlags = 0;
    traceParams.SourceId = *pProviderID;
    traceParams.FilterDescCount = 0;

    return EnableTraceEx2 (m_handle,
                           pProviderID,
                           EVENT_CONTROL_CODE_ENABLE_PROVIDER,
                           TRACE_LEVEL_VERBOSE,
                           0,
                           0,
                           0,
                           &traceParams) == ERROR_SUCCESS;
}

}   // namespace ETWP