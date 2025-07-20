#ifndef ETWP_ETW_PROFILER_HPP
#define ETWP_ETW_PROFILER_HPP

#include <windows.h>

#include <memory>
#include <string>
#include <vector>

#include "IETWBasedProfiler.hpp"

#include "OS/ETW/CombinedETWSession.hpp"

#include "OS/Process/WaitableProcessGroup.hpp"
#include "OS/Process/ProcessLifetimeObserver.hpp"

#include "OS/Synchronization/CriticalSection.hpp"
#include "OS/Utility/ProfileInterruptRate.hpp"

struct ITraceEvent;

namespace ETWP {

class TraceRelogger;

// Thread-safe class (except when stated otherwise) that can profile a process using ETW. It's also an event source for
//   the lifetime of profiled processes
class ETWProfiler final : public IETWBasedProfiler, public ProcessLifetimeObserver {
public:
    // Meaning of State values:
    //   Unstarted      ->  Initialized, but not started yet
    //   Running        ->  Targets are being profiled
    //   Finished       ->  Target processes exited
    //   Stopped        ->  Profiling was stopped externally
    //   Aborted        ->  Profiling was stopped due to an unexpected condition
    //   Error          ->  Profiling stopped because of an error

    // Controls when profiling should be considered finished
    enum class FinishCriterion {
        AllTargetsFinished, // Relevant only if ProfileChildren is set in the options
        OriginalTargetsFinished
    };

    ETWProfiler (const std::wstring& outputPath,
                 const std::vector<PID>& targetPIDs,
                 const ProfileRate& samplingRate,
                 IETWBasedProfiler::Flags options,
                 FinishCriterion finishCriterion = FinishCriterion::AllTargetsFinished);
    virtual ~ETWProfiler () override;

    virtual bool Start (std::wstring* pErrorOut) override;
    virtual void Stop () override;
    virtual void Abort () override;

    virtual bool IsFinished (State* pResultOut, std::wstring* pErrorOut) override;

    virtual bool EnableProvider (const IETWBasedProfiler::ProviderInfo& providerInfo) override;

    virtual uint32_t GetNumberOfProfiledProcesses () override;

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
    HANDLE m_hWorkerThread;

    std::unique_ptr<CombinedETWSession> m_ETWSession;

    WaitableProcessGroup       m_originalTargets;
    WaitableProcessGroup       m_additionalTargets;
    ProviderInfos              m_userProviders;

    ProfileRate                m_samplingRate;
    IETWBasedProfiler::Options m_options;
    FinishCriterion            m_finishCriterion;
    std::wstring               m_outputPath;

    State m_state;
    std::wstring m_errorFromWorkerThread;

    static unsigned int ProfileHelper (void* instance);

    // ProcessLifetimeObserver
    virtual void ProcessStarted (PID pid, PID parentPID) override;
    virtual void ProcessEnded (PID pid, PID parentPID) override;

    void StopImpl ();   // Not thread safe

    void Profile ();
    bool IsProfiling ();

    std::wstring GenerateETWSessionName ();
    void CloseHandles ();   // Not thread safe

    void  SetErrorFromWorkerThread (const std::wstring& message);
    void  SetState (State newState);
    State GetState ();

    bool HaveAllTargetProcessesExited ();       // Not thread safe
    bool HaveOriginalTargetProcessesExited ();  // Not thread safe
    void WaitForProfilerThread ();              // Not thread safe
};

}   // namespace ETWP

#endif  // #ifndef ETWP_ETW_PROFILER_HPP