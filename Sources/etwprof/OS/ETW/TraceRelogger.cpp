#include "TraceRelogger.hpp"

#include "OS/ETW/ETWSessionInterfaces.hpp"
#include "OS/FileSystem/Utility.hpp"
#include "Log/Logging.hpp"
#include "Utility/Asserts.hpp"

namespace ETWP {

namespace Impl {

TraceReloggerCallback::TraceReloggerCallback (TraceRelogger* parent):
    m_refCount (0),
    m_parent (parent)  
{
}

STDMETHODIMP TraceReloggerCallback::QueryInterface (const IID& iid, void** obj)
{
    if (iid == IID_IUnknown) {
        *obj = static_cast<IUnknown*> (this);
    } else if (iid == __uuidof (ITraceEventCallback)) {
        *obj = static_cast<ITraceEventCallback*> (this);
    } else {
        *obj = nullptr;

        return E_NOINTERFACE;
    }

    return S_OK;
}

STDMETHODIMP_ (ULONG) TraceReloggerCallback::AddRef (void)
{
    return InterlockedIncrement (&m_refCount);
}

STDMETHODIMP_ (ULONG) TraceReloggerCallback::Release ()
{
    ULONG ucount = InterlockedDecrement (&m_refCount);
    if (ucount == 0) {
        delete this;
    }

    return ucount;
}

STDMETHODIMP TraceReloggerCallback::OnBeginProcessTrace (ITraceEvent* /*headerEvent*/, ITraceRelogger* /*relogger*/)
{
    return S_OK;
}

STDMETHODIMP TraceReloggerCallback::OnEvent (ITraceEvent* event, ITraceRelogger* /*relogger*/)
{
    m_parent->m_pEventFilter->FilterEvent (event, m_parent);

    return S_OK;
}

STDMETHODIMP TraceReloggerCallback::OnFinalizeProcessTrace (ITraceRelogger* /*relogger*/)
{
    return S_OK;
}

}   // namespace Impl

IEventFilter::~IEventFilter()
{
}

TraceRelogger::InitException::InitException (const std::wstring& msg):
    Exception (msg)
{

}

TraceRelogger::TraceRelogger (IEventFilter* pEventFilter,
                              const std::wstring& outFilePath,
                              bool compress):
    m_pEventFilter (pEventFilter)
{
    // COM should be initialized by clients, but let's be sure...
    if (FAILED (CoInitializeEx (nullptr, COINIT_MULTITHREADED)))
        throw InitException (L"Unable to initialize COM!");

    // Make sure to uninitialize COM *after* m_relogger is destroyed
    m_comUninitializer = std::make_unique<OnExit> ([]() { CoUninitialize (); });

    if (FAILED (m_relogger.CoCreateInstance (CLSID_TraceRelogger, nullptr, CLSCTX_INPROC_SERVER)))
        throw InitException (L"Unable to create ITraceRelogger COM object!");

    m_relogger->SetCompressionMode (compress);

    CComBSTR pathCopy (SysAllocString (outFilePath.c_str ()));
    if (FAILED (m_relogger->SetOutputFilename (pathCopy)))
        throw InitException (L"Unable to set outpath path on ITraceRelogger COM object!");

    m_callbackImpl.reset (new Impl::TraceReloggerCallback (this));

    m_relogger->RegisterCallback (m_callbackImpl.get ());
}

TraceRelogger::~TraceRelogger ()
{
    // m_comUninitializer will uninitialize COM (after the dtor of m_relogger has run)
}

bool TraceRelogger::AddRealTimeSession (const IETWSession& session, std::wstring* pErrorOut)
{
    CComBSTR sessionNameCopy (SysAllocString (session.GetName ().c_str ()));
    if (FAILED (m_relogger->AddRealtimeTraceStream (sessionNameCopy, nullptr, &m_reloggerHandle))) {
        *pErrorOut = L"Unable to add ETW session to ITraceRelogger COM object!";

        return false;
    } else {
        Log (LogSeverity::Debug, L"Relogger handle (session): " + std::to_wstring (m_reloggerHandle));

        return true;
    }
}

bool TraceRelogger::AddTraceFile (const std::wstring& traceFilePath, std::wstring* pErrorOut)
{
    if (ETWP_ERROR (!PathValid (traceFilePath) || !PathExists (traceFilePath))) {
        *pErrorOut = L"Invalid trace file path given to relogger: " + traceFilePath;

        return false;
    }

    CComBSTR pathCopy (SysAllocString (traceFilePath.c_str ()));
    if (FAILED (m_relogger->AddLogfileTraceStream (pathCopy, nullptr, &m_reloggerHandle))) {
        *pErrorOut = L"Unable to add trace file to ITraceRelogger COM object!";

        return false;
    } else {
        Log (LogSeverity::Debug, L"Relogger handle (trace file): " + std::to_wstring (m_reloggerHandle));

        return true;
    }
}

bool TraceRelogger::CreateEventInstance (ITraceEvent** ppEventOut, std::wstring* pErrorOut)
{
    ETWP_ASSERT (ppEventOut != nullptr);

    if (FAILED (m_relogger->CreateEventInstance (m_reloggerHandle,
                                                 0, // Create a "modern" aka. Vista aka. Crimson event
                                                 ppEventOut)))
    {
        *pErrorOut = L"Unable to create new event from ITraceRelogger COM object!";

        return false;
    } else {
        return true;
    }
}

bool TraceRelogger::Inject (ITraceEvent* pEvent, std::wstring* pErrorOut)
{
    if (FAILED (m_relogger->Inject (pEvent))) {
        *pErrorOut = L"Unable to inject event into ITraceRelogger COM object!";

        return false;
    } else {
        return true;
    }
}

bool TraceRelogger::StartRelogging (std::wstring* pErrorOut)
{
    if (FAILED (m_relogger->ProcessTrace ())) {
        *pErrorOut = L"Unable to start ITraceRelogger COM object!";

        return false;
    } else {
        return true;
    }
}

}   // namespace ETWP