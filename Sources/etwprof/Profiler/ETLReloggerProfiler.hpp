#ifndef ETWP_ETL_RELOGGER_PROFILER_HPP
#define ETWP_ETL_RELOGGER_PROFILER_HPP

#include <memory>
#include <string>

#include "OS/Synchronization/CriticalSection.hpp"

#include "Profiler/IProfiler.hpp"

struct ITraceEvent;

namespace ETWP {

class TraceRelogger;

// Thread-safe class (except when stated otherwise) that can emulate ETW
//   profiling by relogging an ETL file
class ETLReloggerProfiler final : public IProfiler {
public:
    using Flags = uint8_t;

    enum Options : Flags {
        Default         = 0b000,
        RecordCSwitches = 0b001,     // Record context switch information
        Compress        = 0b010,     // Compress result ETL with ETW's built-in compression
        Debug           = 0b100      // Preserve intermediate ETL files
    };

    ETLReloggerProfiler (const std::wstring& inputPath,
                         const std::wstring& outputPath,
                         DWORD target,
                         Flags options);
    virtual ~ETLReloggerProfiler () override;

    virtual bool Start (std::wstring* pErrorOut) override;
    virtual void Stop () override;
    virtual void Abort () override;

    virtual bool IsFinished (ResultCode* pResultOut, std::wstring* pErrorOut) override;

private:
    // See the comment in ETLProfiler.hpp as for why we need two locks
    CriticalSection m_lock;         // Lock guarding everything, except m_result and m_errorFromWorkerThread
    CriticalSection m_resultLock;   // Lock guarding m_result and m_errorFromWorkerThread

    HANDLE m_hWorkerThread;

    DWORD m_targetPID;

    std::wstring m_inputPath;
    std::wstring m_outputPath;

    bool    m_profiling;
    Options m_options;

    ResultCode   m_result;
    std::wstring m_errorFromWorkerThread;

    void StopImpl ();   // Not thread safe

    static unsigned int ProfileHelper (void* instance);
    void Profile ();

    void CloseHandles ();
};

}   // namespace ETWP

#endif  // #ifndef ETWP_ETL_RELOGGER_PROFILER_HPP