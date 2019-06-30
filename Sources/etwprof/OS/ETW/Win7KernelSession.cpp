#include "Win7KernelSession.hpp"

#include "OS/ETW/ETWSessionCommon.hpp"
#include "Utility/Asserts.hpp"

namespace ETWP {

Win7KernelSession::Win7KernelSession (ULONG kernelFlags):
    m_handle (NULL),
    m_flags (kernelFlags),
    m_properties (nullptr),
    m_started (false)
{

}

Win7KernelSession::~Win7KernelSession ()
{
    if (m_started) {
        ETWP_VERIFY (StopETWSession (m_handle, KERNEL_LOGGER_NAMEW, m_properties.get ()));
    }
}

std::wstring Win7KernelSession::GetName () const
{
    return KERNEL_LOGGER_NAMEW;
}

bool Win7KernelSession::Start ()
{
    if (ETWP_ERROR (m_started))
        return true;

    // On Windows 7, there can only be one kernel logger, and with a fixed name (KERNEL_LOGGER_NAMEW)
    m_started = StartRealTimeETWSession (KERNEL_LOGGER_NAMEW,
                                         EVENT_TRACE_REAL_TIME_MODE,
                                         m_flags,
                                         &m_handle,
                                         &m_properties);

    return m_started;
}

bool Win7KernelSession::Stop ()
{
    if (ETWP_ERROR (!m_started))
        return true;

    m_started = !StopETWSession (m_handle, KERNEL_LOGGER_NAMEW, m_properties.get ());
    
    return !m_started;
}

TRACEHANDLE Win7KernelSession::GetNativeHandle () const
{
    return m_handle;
}

ULONG Win7KernelSession::GetKernelFlags () const
{
    return m_flags;
}

}   // namespace ETWP