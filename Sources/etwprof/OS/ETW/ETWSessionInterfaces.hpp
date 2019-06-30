#ifndef ETWP_ETW_SESSION_INTERFACES_HPP
#define ETWP_ETW_SESSION_INTERFACES_HPP

#define INITGUID
#include <windows.h>
#include <evntrace.h>

#include <string>

#include "Utility/Macros.hpp"

namespace ETWP {

class IETWSession {
public:
    ETWP_DISABLE_COPY_AND_MOVE (IETWSession);

    IETWSession () = default;

    virtual std::wstring GetName () const = 0;

    virtual bool Start () = 0;
    virtual bool Stop () = 0;

    virtual TRACEHANDLE GetNativeHandle () const = 0;

    virtual ~IETWSession ();
};

class IKernelETWSession : public virtual IETWSession {
public:
    virtual ULONG GetKernelFlags () const = 0;

    virtual ~IKernelETWSession ();
};

class INormalETWSession : public virtual IETWSession {
public:
    virtual bool EnableProvider (LPCGUID pProviderID,
                                 bool collectStacks,
                                 UCHAR level = TRACE_LEVEL_VERBOSE,
                                 ULONGLONG mathcAnyKeyword = 0,
                                 ULONGLONG mathcAllKeyword = 0) = 0;

    virtual bool DisableProvider (LPCGUID pProviderID) = 0;

    virtual ~INormalETWSession ();
};

}   // namespace ETWP

#endif  // #ifndef ETWP_ETW_SESSION_INTERFACES_HPP