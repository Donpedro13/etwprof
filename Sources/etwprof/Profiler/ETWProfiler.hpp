#ifndef ETWP_ETW_PROFILER_HPP
#define ETWP_ETW_PROFILER_HPP

#include <windows.h>

#include <memory>
#include <string>
#include <vector>

#include "IETWBasedProfiler.hpp"

#include "OS/ETW/ETWSessionInterfaces.hpp"
#include "OS/Synchronization/CriticalSection.hpp"
#include "OS/Utility/ProfileInterruptRate.hpp"

struct ITraceEvent;

namespace ETWP {

class TraceRelogger;

// Thread-safe class (except when stated otherwise) that can profile a process
//   using ETW
class ETWProfiler final : public IETWBasedProfiler {
public:
    // Meaning of ResultCode values:
    //   Unstarted      ->  Initialized, but not started yet
    //   Running        ->  Target is being profiled
    //   Finished       ->  Target process exited
    //   Stopped        ->  Profiling was stopped externally
    //   Aborted        ->  Profiling was stopped due to an unexpected condition
    //   Error          ->  Profiling stopped because of an error

    ETWProfiler (const std::wstring& outputPath,
                 DWORD target,
                 const ProfileRate& samplingRate,
                 IETWBasedProfiler::Flags options);
    virtual ~ETWProfiler () override;

    virtual bool Start (std::wstring* pErrorOut) override;
    virtual void Stop () override;
    virtual void Abort () override;

    virtual bool IsFinished (ResultCode* pResultOut, std::wstring* pErrorOut) override;

    virtual bool EnableProvider (const IETWBasedProfiler::ProviderInfo& providerInfo) override;

private:
    using ProviderInfos = std::vector<IETWBasedProfiler::ProviderInfo>;

    // We need two locks to avoid deadlocks (consider: the profiler is being stopped from thread A, then the profiler
    //   thread B fails stopping with an error (e.g. trace merge fails), but needs to lock the lock to do so.
    //   Since stopping waits for the profiling thread synchronously, this kind of situation cannot be handled without
    //   fine-grained locking)
    //
    // Make sure to acquire these in order, if you need both
    CriticalSection m_lock;         // Lock guarding everything, except m_result and m_errorFromWorkerThread
    CriticalSection m_resultLock;   // Lock guarding m_result and m_errorFromWorkerThread

    HANDLE m_hTargetProcess;
    HANDLE m_hWorkerThread;

    std::unique_ptr<IKernelETWSession> m_ETWSession;

    DWORD                      m_targetPID;
    ProviderInfos              m_userProviders;

    ProfileRate                m_samplingRate;
    IETWBasedProfiler::Options m_options;
    std::wstring               m_outputPath;

    bool m_profiling;

    ResultCode   m_result;
    std::wstring m_errorFromWorkerThread;

    static unsigned int ProfileHelper (void* instance);

    void StopImpl ();   // Not thread safe

    void Profile ();

    std::wstring GenerateETWSessionName ();
    void CloseHandles ();
};

}   // namespace ETWP

#endif  // #ifndef ETWP_ETW_PROFILER_HPP