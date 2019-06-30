#ifndef ETWP_ETL_RELOGGER_PROFILER_HPP
#define ETWP_ETL_RELOGGER_PROFILER_HPP

#include <windows.h>

#include <memory>
#include <string>
#include <vector>

#include "OS/Synchronization/CriticalSection.hpp"

#include "IETWBasedProfiler.hpp"

struct ITraceEvent;

namespace ETWP {

class TraceRelogger;

// Thread-safe class (except when stated otherwise) that can emulate ETW
//   profiling by relogging an ETL file
class ETLReloggerProfiler final : public IETWBasedProfiler {
public:
    ETLReloggerProfiler (const std::wstring& inputPath,
                         const std::wstring& outputPath,
                         DWORD target,
                         IETWBasedProfiler::Flags options);
    virtual ~ETLReloggerProfiler () override;

    virtual bool Start (std::wstring* pErrorOut) override;
    virtual void Stop () override;
    virtual void Abort () override;

    virtual bool IsFinished (ResultCode* pResultOut, std::wstring* pErrorOut) override;

    virtual bool EnableProvider (const IETWBasedProfiler::ProviderInfo& providerInfo) override;

private:
    // See the comment in ETLProfiler.hpp as for why we need two locks
    CriticalSection m_lock;         // Lock guarding everything, except m_result and m_errorFromWorkerThread
    CriticalSection m_resultLock;   // Lock guarding m_result and m_errorFromWorkerThread

    HANDLE m_hWorkerThread;

    DWORD                                        m_targetPID;
    std::vector<IETWBasedProfiler::ProviderInfo> m_userProviders;

    std::wstring m_inputPath;
    std::wstring m_outputPath;

    bool                       m_profiling;
    IETWBasedProfiler::Options m_options;

    ResultCode   m_result;
    std::wstring m_errorFromWorkerThread;

    void StopImpl ();   // Not thread safe

    static unsigned int ProfileHelper (void* instance);
    void Profile ();

    void CloseHandles ();
};

}   // namespace ETWP

#endif  // #ifndef ETWP_ETL_RELOGGER_PROFILER_HPP