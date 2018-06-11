#ifndef ETWP_WIN8_PLUS_KERNEL_SESSION_HPP
#define ETWP_WIN8_PLUS_KERNEL_SESSION_HPP

#include <memory>
#include <string>

#include "ETWSessionInterfaces.hpp"

namespace ETWP {

class Win8PlusKernelSession : public INormalETWSession,
                              public IKernelETWSession {
public:
    ETWP_DISABLE_COPY_AND_MOVE (Win8PlusKernelSession);

    Win8PlusKernelSession (const std::wstring& name, ULONG kernelFlags);

    virtual ~Win8PlusKernelSession () override;

    virtual std::wstring GetName () const override;

    virtual bool Start () override;
    virtual bool Stop () override;

    virtual TRACEHANDLE GetNativeHandle () const override;

    virtual bool EnableProvider (LPCGUID pProviderID,
                                 UCHAR level = TRACE_LEVEL_VERBOSE,
                                 ULONGLONG mathcAnyKeyword = 0,
                                 ULONGLONG mathcAllKeyword = 0) override;

    virtual bool DisableProvider (LPCGUID pProviderID) override;

    virtual ULONG GetKernelFlags () const override;

protected:
    TRACEHANDLE m_handle;
    std::wstring m_name;
    ULONG m_flags;
    std::unique_ptr<EVENT_TRACE_PROPERTIES> m_properties;
    bool m_started;
};

}   // namespace ETWP

#endif  // #ifndef ETWP_WIN8_PLUS_KERNEL_SESSION_HPP