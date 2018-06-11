#ifndef ETWP_NORMAL_ETW_SESSION_HPP
#define ETWP_NORMAL_ETW_SESSION_HPP

#include "ETWSessionInterfaces.hpp"

#include <memory>
#include <string>

namespace ETWP {

class NormalETWSession : public INormalETWSession {
public:
    NormalETWSession (const std::wstring& name);

    virtual ~NormalETWSession () override;

    virtual std::wstring GetName () const override;

    virtual bool Start () override;
    virtual bool Stop () override;

    virtual TRACEHANDLE GetNativeHandle () const override;

    virtual bool EnableProvider (LPCGUID pProviderID,
                                 UCHAR level = TRACE_LEVEL_VERBOSE,
                                 ULONGLONG mathcAnyKeyword = 0,
                                 ULONGLONG mathcAllKeyword = 0) override;

    virtual bool DisableProvider (LPCGUID pProviderID) override;

protected:
    TRACEHANDLE m_handle;
    std::wstring m_name;
    std::unique_ptr<EVENT_TRACE_PROPERTIES> m_properties;
    bool m_started;
};

}   // namespace ETWP

#endif  // #ifndef ETWP_NORMAL_ETW_SESSION_HPP