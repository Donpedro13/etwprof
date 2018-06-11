#ifndef ETWP_WIN7_KERNEL_SESSION_HPP
#define ETWP_WIN7_KERNEL_SESSION_HPP

#include <memory>
#include <string>

#include "ETWSessionInterfaces.hpp"

#include "Utility/Macros.hpp"

namespace ETWP {

class Win7KernelSession : public IKernelETWSession {
public:
    ETWP_DISABLE_COPY_AND_MOVE (Win7KernelSession);

    Win7KernelSession (ULONG kernelFlags);

    virtual ~Win7KernelSession () override;

    virtual std::wstring GetName () const override;

    virtual bool Start () override;
    virtual bool Stop () override;

    virtual TRACEHANDLE GetNativeHandle () const override;

    virtual ULONG GetKernelFlags () const override;

protected:
    TRACEHANDLE m_handle;
    ULONG m_flags;
    std::unique_ptr<EVENT_TRACE_PROPERTIES> m_properties;
    bool m_started;
};

}   // namespace ETWP

#endif  // #ifndef ETWP_WIN7_KERNEL_SESSION_HPP