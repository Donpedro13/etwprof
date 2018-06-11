#ifndef ETWP_TRACE_RELOGGER_HPP
#define ETWP_TRACE_RELOGGER_HPP

#include <windows.h>

#include <cguid.h>
#include <atlbase.h>
#include <atlcom.h>

#include <relogger.h>

#include <functional>
#include <memory>
#include <string>

#include "Utility/Exception.hpp"
#include "Utility/Macros.hpp"
#include "Utility/OnExit.hpp"

namespace ETWP {

class IETWSession;
class TraceRelogger;

namespace Impl {

class TraceReloggerCallback : public ITraceEventCallback {
public:
    ETWP_DISABLE_COPY_AND_MOVE (TraceReloggerCallback);

    TraceReloggerCallback (TraceRelogger* parent, void* context);

    STDMETHODIMP QueryInterface (const IID& iid, void **obj);
    STDMETHODIMP_ (ULONG) AddRef ();
    STDMETHODIMP_ (ULONG) Release ();

    STDMETHODIMP OnBeginProcessTrace (ITraceEvent* headerEvent, ITraceRelogger* relogger);
    STDMETHODIMP OnEvent (ITraceEvent* event, ITraceRelogger* relogger);
    STDMETHODIMP OnFinalizeProcessTrace (ITraceRelogger* relogger);

private:
    DWORD          m_refCount;
    void*          m_context;

    TraceRelogger* m_parent;
};

}   // namespace Impl

// 1.) Create an instance with your callback
// 2.) Add a session or a trace file to relog
// 3.) Start relogging; your callback will be called back with events
// 4.) Filter/add events as you like, by calling Inject
class TraceRelogger final {
public:
    ETWP_DISABLE_COPY_AND_MOVE (TraceRelogger);

    friend Impl::TraceReloggerCallback;

    class InitException : public Exception {
    public:
        InitException (const std::wstring& msg);
    };

    using EventCallback = std::function<void (ITraceEvent*, TraceRelogger*, void*)>;

    // Might throw InitException
    TraceRelogger (const EventCallback& eventCallback,
                   const std::wstring& outFilePath,
                   void* context,
                   bool compress);
    ~TraceRelogger ();

    bool AddRealTimeSession (const IETWSession& session, std::wstring* pErrorOut);
    bool AddTraceFile (const std::wstring& traceFilePath, std::wstring* pErrorOut);

    bool CreateEventInstance (ITraceEvent** ppEventOut, std::wstring* pErrorOut);

    bool Inject (ITraceEvent* pEvent, std::wstring* pErrorOut);

    bool StartRelogging (std::wstring* pErrorOut);

private:
    EventCallback m_callback;
    void*         m_context;

    std::unique_ptr<OnExit> m_comUninitializer;

    CComPtr<ITraceRelogger>                      m_relogger;
    std::unique_ptr<Impl::TraceReloggerCallback> m_callbackImpl;
    TRACEHANDLE                                  m_reloggerHandle;
};

}   // namespace ETWP

#endif  // #ifndef ETWP_TRACE_RELOGGER_HPP