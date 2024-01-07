#include "ETLReloggerProfiler.hpp"

#include <process.h>

#include "Log/Logging.hpp"

#include "OS/ETW/TraceRelogger.hpp"
#include "OS/FileSystem/Utility.hpp"
#include "OS/Process/Utility.hpp"
#include "OS/Synchronization/LockableGuard.hpp"
#include "OS/Utility/Win32Utils.hpp"

#include "ProfilerCommon.hpp"

#include "Utility/Asserts.hpp"

namespace ETWP {

ETLReloggerProfiler::ETLReloggerProfiler (const std::wstring& inputPath,
                                          const std::wstring& outputPath,
                                          DWORD target,
                                          IETWBasedProfiler::Flags options):
    m_lock (),
    m_hWorkerThread (nullptr),
    m_targetPID (target),
    m_userProviders (),
    m_inputPath (inputPath),
    m_outputPath (outputPath),
    m_profiling (false),
    m_options (static_cast<Options> (options)),
    m_state (State::Unstarted),
    m_errorFromWorkerThread ()
{
    if (!PathValid (m_inputPath))
        throw InitException (L"Input ETL path is invalid!");

    if (!PathValid (m_outputPath))
        throw InitException (L"Output ETL path is invalid!");

    if (m_targetPID == 0)
        throw InitException (L"Target PID is invalid!");

    if (m_options & StackCache)
        throw InitException (L"ETW stack caching is not supported by ETLReloggerProfiler!");

    if (IsProcessElevated ()) {
        Log (LogSeverity::Info,
             L"You are running elevated, but ETLReloggerProfiler does not require administrative privileges!");
    }
}

ETLReloggerProfiler::~ETLReloggerProfiler ()
{
    LockableGuard guard (&m_lock);

    if (m_profiling)
        Stop ();

    // This HANDLE should be closed by Stop(Impl)
    ETWP_ASSERT (m_hWorkerThread == nullptr);
}

bool ETLReloggerProfiler::Start (std::wstring* pErrorOut)
{
    LockableGuard lockGuard (&m_lock);

    if (ETWP_ERROR (m_profiling))
        return false;

    unsigned int workerTID;
    m_hWorkerThread = reinterpret_cast<HANDLE> (_beginthreadex (nullptr,
                                                                0,
                                                                ProfileHelper,
                                                                this,
                                                                0,
                                                                &workerTID));

    if (ETWP_ERROR (m_hWorkerThread == 0)) {
        *pErrorOut = L"Unable to launch ETL relogger worker thread!";

        return false;
    }

    Log (LogSeverity::Debug, L"ETL relogger thread started with TID " + std::to_wstring (workerTID));

    m_profiling = true;

    LockableGuard resultLockGuard (&m_resultLock);
    m_state = State::Running;

    return true;
}

void ETLReloggerProfiler::Stop ()
{
    LockableGuard lockGuard (&m_lock);

    StopImpl ();

    LockableGuard resultLockGuard (&m_resultLock);
    if (m_state != State::Error)  // Stopping can result in an error; do not change the result in this case
        m_state = State::Stopped;
}

void ETLReloggerProfiler::Abort ()
{
    LockableGuard lockGuard (&m_lock);

    StopImpl ();

    LockableGuard resultLockGuard (&m_resultLock);
    // Stopping can result in an error, but we discard the result anyways, so we don't care if we overwrite the error
    //   state with State::Aborted
    m_state = State::Aborted;
}

bool ETLReloggerProfiler::IsFinished (State* pResultOut, std::wstring* pErrorOut)
{
    LockableGuard lockGuard (&m_lock);

    if (!m_profiling) {
        LockableGuard resultLockGuard (&m_resultLock);

        *pResultOut = m_state;

        if (m_state == IProfiler::State::Error)
            *pErrorOut = m_errorFromWorkerThread;

        return true;
    }

    ETWP_ASSERT (m_hWorkerThread != nullptr);

    // Poll thread
    switch (Win32::WaitForObject(m_hWorkerThread, 0)) {
        case Win32::WaitResult::Signaled: {
            LockableGuard resultLockGuard (&m_resultLock);

            m_profiling = false;
            if (m_state == IProfiler::State::Running || m_state == IProfiler::State::Unstarted)
                m_state = IProfiler::State::Finished;

            CloseHandles ();

            break;
        }

        case Win32::WaitResult::Timeout:
            break;

        case Win32::WaitResult::Failed:
            ETWP_DEBUG_BREAK_STR (L"Wait failed on profiler thread!");
            // Hope for the best...
            break;
    }

    if (!m_profiling) {
        LockableGuard resultLockGuard (&m_resultLock);

        *pResultOut = m_state;

        if (m_state == IProfiler::State::Error)
            *pErrorOut = m_errorFromWorkerThread;
    }

    return !m_profiling;
}

bool ETLReloggerProfiler::EnableProvider (const IETWBasedProfiler::ProviderInfo& providerInfo)
{
    m_userProviders.push_back (providerInfo);

    return true;
}

void ETLReloggerProfiler::StopImpl ()
{
    if (ETWP_ERROR (!m_profiling))
        return;

    // We could stop the relogger by calling Cancel on it. However, we can't access the relogger right here, so we
    //   can't do that w/o code changes. This way Stop will just wait for relogging to finish synchronously, which is
    //  bad UX. On the other hand, relogging is pretty fast, so this is not a huge problem.
    
    ETWP_ASSERT (m_hWorkerThread != nullptr);

    // Wait for profiler thread synchronously
    switch (Win32::WaitForObject(m_hWorkerThread, INFINITE)) {
        case Win32::WaitResult::Signaled:
            break;

        default:
            ETWP_DEBUG_BREAK_STR(L"Impossible value returned from"
                                 L"WaitForSingleObject in profiler!");
    }

    m_profiling = false;
}

unsigned int ETLReloggerProfiler::ProfileHelper (void* instance)
{
    ETLReloggerProfiler* pInstance = static_cast<ETLReloggerProfiler*> (instance);

    pInstance->Profile ();

    return 0;
}

void ETLReloggerProfiler::Profile ()
{
    LockableGuard lockGuard (&m_lock);

    // Create copy of data needed by the filtering relogger callback, so it can run lockless
    ProfileFilterData filterData = { {},
                                     {m_userProviders.begin (), m_userProviders.end () },
                                     {},
                                     {m_targetPID},
                                     bool (m_options & RecordCSwitches) };

    // These will be used later, we create a copy as well (so no locking will be required)
    std::wstring outputPath = m_outputPath;
    std::wstring rawOutputPath = m_outputPath + L".raw.etl";
    bool debug = m_options & Debug;
    bool compress = m_options & Compress;

    try {
        TraceRelogger filteringRelogger (FilterEventForProfiling,
                                         rawOutputPath,
                                         &filterData,
                                         m_options & Compress);

        std::wstring errorMsg;
        if (ETWP_ERROR (!filteringRelogger.AddTraceFile (m_inputPath, &errorMsg))) {
            LockableGuard resultLockGuard (&m_resultLock);

            m_state = State::Error;
            m_errorFromWorkerThread = L"Unable to add input ETL file to filtering relogger: " + errorMsg;

            return;
        }

        // Note: we unlock the lock, so the relogging process can run lock free
        lockGuard.Unlock ();

        // This will call back FilterEventForProfiling
        if (ETWP_ERROR (!filteringRelogger.StartRelogging (&errorMsg))) {
            LockableGuard resultLockGuard (&m_resultLock);

            m_state = State::Error;
            m_errorFromWorkerThread = L"Unable to start relogger: " + errorMsg;

            return;
        }
    } catch (const TraceRelogger::InitException& e) {
        LockableGuard resultLockGuard (&m_resultLock);

        m_state = State::Error;
        m_errorFromWorkerThread = L"Unable to construct filtering relogger: " + e.GetMsg ();

        return;
    }

    // This point is reached, when the original ETW session is stopped

    OnExit rawETLDeleter ([&rawOutputPath]() {
        // Delete temporary (unmerged) file
        if (ETWP_ERROR (!FileDelete (rawOutputPath)))
            Log (LogSeverity::Debug, L"Unable to delete raw ETL file!");
    });

    if (debug)
        rawETLDeleter.Deactivate ();

    DWORD mergeFlags = EVENT_TRACE_MERGE_EXTENDED_DATA_IMAGEID |
                       EVENT_TRACE_MERGE_EXTENDED_DATA_EVENT_METADATA;
    if (compress)
        mergeFlags |= EVENT_TRACE_MERGE_EXTENDED_DATA_COMPRESS_TRACE;

    std::wstring mergeErrorMsg;
    if (ETWP_ERROR (!MergeTrace (rawOutputPath, mergeFlags, outputPath, &mergeErrorMsg))) {
        LockableGuard resultLockGuard (&m_resultLock);

        m_state = State::Error;
        m_errorFromWorkerThread = L"Debug info merge failed: " + mergeErrorMsg;

        return;
    }
}

void ETLReloggerProfiler::CloseHandles ()
{
    if (m_hWorkerThread != nullptr) {
        CloseHandle (m_hWorkerThread);
        m_hWorkerThread = nullptr;
    }
}

}   // namespace ETWP