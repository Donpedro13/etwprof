#include "ETWProfiler.hpp"

#include <process.h>
#include <string>

#include "Log/Logging.hpp"

#include "OS/ETW/ETWConstants.hpp"
#include "OS/ETW/NormalETWSession.hpp"
#include "OS/ETW/TraceRelogger.hpp"
#include "OS/ETW/Win7KernelSession.hpp"
#include "OS/ETW/Win8PlusKernelSession.hpp"

#include "OS/FileSystem/Utility.hpp"

#include "OS/Process/Utility.hpp"

#include "OS/Synchronization/LockableGuard.hpp"

#include "OS/Version/WinVersion.hpp"

#include "ProfilerCommon.hpp"

#include "Utility/Asserts.hpp"
#include "Utility/GUID.hpp"
#include "Utility/OnExit.hpp"

namespace ETWP {

ETWProfiler::ETWProfiler (const std::wstring& outputPath,
                          DWORD target,
                          const ProfileRate& samplingRate,
                          IETWBasedProfiler::Flags options):
    m_lock (),
    m_resultLock (),
    m_hTargetProcess (nullptr),
    m_hWorkerThread (nullptr),
    m_ETWSession (nullptr),
    m_targetPID (target),
    m_userProviders (),
    m_samplingRate (samplingRate),
    m_options (static_cast<Options> (options)),
    m_outputPath (outputPath),
    m_profiling (false),
    m_result (ResultCode::Unstarted),
    m_errorFromWorkerThread ()
{
    if (!PathValid (m_outputPath))
        throw InitException (L"Output ETL path is invalid!");

    if (m_targetPID == 0)
        throw InitException (L"Target PID is invalid!");

    m_hTargetProcess = OpenProcess (SYNCHRONIZE | PROCESS_QUERY_INFORMATION, FALSE, target);

    if (m_hTargetProcess == nullptr)
        throw InitException (L"Unable to open target process (OpenProcess failed)!");

    // In order to be able to create a kernel logger session, we need administrative privileges
    if (!IsProcessElevated ()) {
        throw InitException (L"Process is not elevated! For ETW profiling, you need to run this program as "
            L"an Administrator!");
    }

    // To be able to enable the sampling profiler in the ETW session, we need to adjust the process' token
    bool profilePrivPresent;
    if (ETWP_ERROR (!IsProfilePrivilegePresentInToken (&profilePrivPresent))) {
        throw InitException (L"Unable to determine whether the system profile privilege is in the process' token!");
    } else {
        if (!profilePrivPresent)
            throw InitException (L"The system profile privilege is missing from the process' token! It is required to "
                L"create an ETW kernel logger for profiling!");
    }

    // Create the heart and soul of this class, an ETW kernel logger
    ULONG kProfilingFlags = EVENT_TRACE_FLAG_PROCESS    |
                            EVENT_TRACE_FLAG_THREAD     |
                            EVENT_TRACE_FLAG_IMAGE_LOAD |
                            EVENT_TRACE_FLAG_PROFILE;

    if (m_options & RecordCSwitches)
        kProfilingFlags |= EVENT_TRACE_FLAG_CSWITCH | EVENT_TRACE_FLAG_DISPATCHER;

    if (GetWinVersion () >= BaseWinVersion::Win8)
        m_ETWSession.reset (new Win8PlusKernelSession (GenerateETWSessionName (), kProfilingFlags));
    else
        m_ETWSession.reset (new Win7KernelSession (kProfilingFlags));

    Log (LogSeverity::Debug, L"ETW session name of profiler will be: " + m_ETWSession->GetName ());
}

ETWProfiler::~ETWProfiler ()
{
    LockableGuard<CriticalSection> lockGuard (&m_lock);

    if (m_profiling)
        Stop ();

    // These HANDLEs should be closed by Stop(Impl)
    ETWP_ASSERT (m_hTargetProcess == nullptr);
    ETWP_ASSERT (m_hWorkerThread == nullptr);
}

bool ETWProfiler::Start (std::wstring* pErrorOut)
{
    LockableGuard<CriticalSection> lockGuard (&m_lock);

    if (ETWP_ERROR (m_profiling))
        return true;

    ETWP_ASSERT (pErrorOut != nullptr);
    ETWP_ASSERT (m_hTargetProcess != nullptr);
    ETWP_ASSERT (m_ETWSession != nullptr);

    // Take care of profile rate
    ProfileRate currentRate (InvalidProfileRate);
    if (ETWP_ERROR (!GetGlobalSamplingRate (&currentRate))) {
        Log (LogSeverity::Warning, L"Unable to query current sampling rate!");
    } else {
        Log (LogSeverity::Info, L"Current sampling rate is " +
            std::to_wstring (currentRate.count ()) + L" ms / " +
            std::to_wstring (ConvertSamplingRateToHz (currentRate)) + L" Hz");
    }

    if (m_samplingRate != InvalidProfileRate) {
        if (ETWP_ERROR (!SetGlobalSamplingRate (m_samplingRate))) {
            *pErrorOut = L"Unable to set sampling rate!";

            return false;
        }

        // Reread new sampling rate
        if (ETWP_VERIFY (GetGlobalSamplingRate (&currentRate))) {
            Log (LogSeverity::Debug, L"Re-read sampling rate is " +
                std::to_wstring (currentRate.count ()) + L" ms / " +
                std::to_wstring (ConvertSamplingRateToHz (currentRate)) + L" Hz");
        }
    }

    unsigned int workerTID;
    m_hWorkerThread = reinterpret_cast<HANDLE> (_beginthreadex (nullptr,
                                                                0,
                                                                ProfileHelper,
                                                                this,
                                                                0,
                                                                &workerTID));

    if (ETWP_ERROR (m_hWorkerThread == 0)) {
        *pErrorOut = L"Unable to launch profiler worker thread!";

        return false;
    }

    Log (LogSeverity::Debug, L"Profiler thread started with TID " +
         std::to_wstring (workerTID));

    m_profiling = true;

    LockableGuard<CriticalSection> resultLockGuard (&m_resultLock);
    m_result = ResultCode::Running;

    return true;
}

void ETWProfiler::Stop ()
{
    LockableGuard<CriticalSection> lockGuard (&m_lock);

    StopImpl ();

    LockableGuard<CriticalSection> resultLockGuard (&m_resultLock);
    if (m_result != ResultCode::Error)  // Stopping can result in an error; do not change the result in this case
        m_result = ResultCode::Stopped;
}

void ETWProfiler::Abort ()
{
    LockableGuard<CriticalSection> lockGuard (&m_lock);

    StopImpl ();

    LockableGuard<CriticalSection> resultLockGuard (&m_resultLock);
    // Stopping can result in an error, but we discard the result anyways, so we don't care if we overwrite the error
    //   state with ResultCode::Aborted
    m_result = ResultCode::Aborted;
}

bool ETWProfiler::IsFinished (ResultCode* pResultOut, std::wstring* pErrorOut)
{
    LockableGuard<CriticalSection> lockGuard (&m_lock);

    if (!m_profiling) {
        LockableGuard<CriticalSection> resultLockGuard (&m_resultLock);

        *pResultOut = m_result;

        if (m_result == IProfiler::ResultCode::Error)
            *pErrorOut = m_errorFromWorkerThread;

        return true;
    }

    ETWP_ASSERT (m_hWorkerThread != nullptr);

    // Poll thread
    DWORD pollResult = WaitForSingleObject (m_hWorkerThread, 0);
    switch (pollResult) {
        case WAIT_OBJECT_0:
            m_profiling = false;

            CloseHandles ();
            break;
        case WAIT_TIMEOUT:
            break;
        case WAIT_FAILED:
            ETWP_DEBUG_BREAK_STR (L"Wait failed on profiler thread!");
            // Hope for the best...
            break;
        default:
            ETWP_DEBUG_BREAK_STR (L"Impossible value returned from"
                                  L"WaitForSingleObject in profiler!");
    }

    // If the profiler thread is still running, poll the target process to see
    //   if it has finished yet
    if (m_profiling) {
        ETWP_ASSERT (m_hTargetProcess != nullptr);

        pollResult = WaitForSingleObject (m_hTargetProcess, 0);
        switch (pollResult) {
            case WAIT_OBJECT_0:
                // If the process finished, we need to stop the profiler thread
                //   manually
                StopImpl ();

                {
                    LockableGuard<CriticalSection> resultLockGuard (&m_resultLock);

                    if (m_result != ResultCode::Error)  // Stopping can result in an error; do not change the result in this case
                        m_result = ResultCode::Finished;
                }
                break;
            case WAIT_TIMEOUT:
                break;
            case WAIT_FAILED:
                ETWP_DEBUG_BREAK_STR (L"Wait failed on target process!");
                // Hope for the best...
                break;
            default:
                ETWP_DEBUG_BREAK_STR (L"Impossible value returned from"
                                      L"WaitForSingleObject in profiler!");
        }
    }

    if (!m_profiling) {
        LockableGuard<CriticalSection> resultLockGuard (&m_resultLock);

        *pResultOut = m_result;

        if (m_result == IProfiler::ResultCode::Error)
            *pErrorOut = m_errorFromWorkerThread;
    }

    return !m_profiling;
}

bool ETWProfiler::EnableProvider (const IETWBasedProfiler::ProviderInfo& providerInfo)
{
    LockableGuard<CriticalSection> lockGuard (&m_lock);

    if (ETWP_ERROR (m_profiling))
        return false;

    m_userProviders.push_back (providerInfo);

    return true;
}

unsigned int ETWProfiler::ProfileHelper (void* instance)
{
    ETWProfiler* pInstance = static_cast<ETWProfiler*> (instance);

    pInstance->Profile ();

    _endthreadex (0);
    
    return 0;   // Never reached, but the compiler isn't aware
}

void ETWProfiler::StopImpl ()
{
    if (ETWP_ERROR (!m_profiling))
        return;

    if (ETWP_ERROR (!m_ETWSession->Stop ())) {
        // We will probably leak an ETW session, which is bad :/
        Log (LogSeverity::Warning, L"Unable to stop ETW session: " + m_ETWSession->GetName () + L"!");
    }

    ETWP_ASSERT (m_hWorkerThread != nullptr);
    
    // Wait for profiler thread synchronously
    DWORD pollResult = WaitForSingleObject (m_hWorkerThread, INFINITE);
    switch (pollResult) {
        case WAIT_OBJECT_0:
            break;
        case WAIT_FAILED:
            // Fallthrough
        case WAIT_TIMEOUT:
            // Fallthrough
        default:
            ETWP_DEBUG_BREAK_STR (L"Impossible value returned from"
                                  L"WaitForSingleObject in profiler!");
    }

    CloseHandles ();

    m_profiling = false;
}

void ETWProfiler::Profile ()
{
    LockableGuard<CriticalSection> lockGuard (&m_lock);

    // Create copy of data needed by the filtering relogger callback, so it can run lockless
    ProfileFilterData filterData = { { 1024 }, { m_userProviders.begin (), m_userProviders.end () }, m_targetPID, bool (m_options & RecordCSwitches) };

    if (ETWP_ERROR (!filterData.userProviders.empty () && GetWinVersion () < BaseWinVersion::Win8)) {
        LockableGuard<CriticalSection> resultLockGuard (&m_resultLock);

        m_result = ResultCode::Error;
        m_errorFromWorkerThread = L"User providers are requested, but Windows version is less than 8!";

        return;
    }

    // These will be used later, we create a copy as well (so no locking will be required)
    std::wstring outputPath = m_outputPath;
    std::wstring rawOutputPath = m_outputPath + L".raw.etl";
    bool debug = m_options & Debug;
    bool compress = m_options & Compress;

    try {
        // Create and set up the relogger before starting the kernel logger. This way we minimize the time between
        // relogging (~consuming) events, and the kernel logger emitting events into buffers
        TraceRelogger filteringRelogger (FilterEventForProfiling,
                                         rawOutputPath,
                                         &filterData,
                                         m_options & Compress);

        if (ETWP_ERROR (!m_ETWSession->Start ())) {
            LockableGuard<CriticalSection> resultLockGuard (&m_resultLock);

            m_result = ResultCode::Error;
            m_errorFromWorkerThread = L"Unable to start ETW session!";

            return;
        }

        OnExit etwSessionDestroyer ([&]() {
            // We could exit ETWProfiler::Profile when the lock is unowned, so we need this
            LockableGuard<CriticalSection> guard (&m_lock);

            m_ETWSession->Stop ();
        });

        // We need stack traces for: - sampled profile
        //                           - ready thread
        //                           - context switch
        CLASSIC_EVENT_ID classes[] = { { PerfInfoGuid, ETWConstants::SampledProfileOpcode, {} },
                                       { ThreadGuid,   ETWConstants::ReadyThreadOpcode,    {} },
                                       { ThreadGuid,   ETWConstants::CSwitchOpcode,        {} } };

        if (ETWP_ERROR (TraceSetInformation (m_ETWSession->GetNativeHandle (),
                                             TraceStackTracingInfo,
                                             classes,
                                             sizeof classes) != ERROR_SUCCESS))
        {
            LockableGuard<CriticalSection> resultLockGuard (&m_resultLock);

            m_result = ResultCode::Error;
            m_errorFromWorkerThread = L"Unable to request stack traces for ETW session!";

            return;
        }

        INormalETWSession* pNormalSession = dynamic_cast<INormalETWSession*> (m_ETWSession.get ()); // Eh...
        ETWP_ASSERT (pNormalSession != nullptr);
        for (auto&& providerInfo : filterData.userProviders) {
            if (ETWP_ERROR (!pNormalSession->EnableProvider (&providerInfo.providerID,
                                                             providerInfo.stack,
                                                             providerInfo.level,
                                                             providerInfo.flags)))
            {
                LockableGuard<CriticalSection> resultLockGuard (&m_resultLock);

                m_result = ResultCode::Error;

                std::wstring guidString = L"UNKNOWN PROVIDER";
                GUIDToString (providerInfo.providerID, &guidString);
                m_errorFromWorkerThread = L"Unable to enable user provider: " + guidString;

                return;
            }
        }
            

        std::wstring errorMsg;
        if (ETWP_ERROR (!filteringRelogger.AddRealTimeSession (*m_ETWSession.get (), &errorMsg))) {
            LockableGuard<CriticalSection> resultLockGuard (&m_resultLock);

            m_result = ResultCode::Error;
            m_errorFromWorkerThread = L"Unable to add ETW session to filter relogger: " + errorMsg;

            return;
        }

        // Note: we unlock the lock, so the relogging process can run lock free
        lockGuard.Release ();

        // This will call back FilterEventForProfiling
        if (ETWP_ERROR (!filteringRelogger.StartRelogging (&errorMsg))) {
            LockableGuard<CriticalSection> resultLockGuard (&m_resultLock);

            m_result = ResultCode::Error;
            m_errorFromWorkerThread = L"Unable to start relogger: " + errorMsg;

            return;
        }

        // At this point, the session should be stopped, so no need for this
        etwSessionDestroyer.Deactivate ();
    } catch (const TraceRelogger::InitException& e) {
        LockableGuard<CriticalSection> resultLockGuard (&m_resultLock);

        m_result = ResultCode::Error;
        m_errorFromWorkerThread = L"Unable to construct filtering relogger: " + e.GetMsg ();

        return;
    }

    OnExit rawETLDeleter ([&rawOutputPath]() {
        // Delete temporary (unmerged) file
        if (ETWP_ERROR (!FileDelete (rawOutputPath)))
            Log (LogSeverity::Debug, L"Unable to delete raw ETL file!");
    });

    if (debug)
        rawETLDeleter.Deactivate ();

    DWORD mergeFlags = EVENT_TRACE_MERGE_EXTENDED_DATA_IMAGEID | EVENT_TRACE_MERGE_EXTENDED_DATA_EVENT_METADATA;
    if (compress)
        mergeFlags |= EVENT_TRACE_MERGE_EXTENDED_DATA_COMPRESS_TRACE;

    std::wstring mergeErrorMsg;
    if (ETWP_ERROR (!MergeTrace (rawOutputPath, mergeFlags, outputPath, &mergeErrorMsg))) {
        LockableGuard<CriticalSection> resultLockGuard (&m_resultLock);

        m_result = ResultCode::Error;
        m_errorFromWorkerThread = L"Debug info merge failed: " + mergeErrorMsg;

        return;
    }
}

std::wstring ETWProfiler::GenerateETWSessionName ()
{
    DWORD ownPID = GetProcessId (GetCurrentProcess ());

    return L"ETWProf_Session_" + std::to_wstring (ownPID);
}

void ETWProfiler::CloseHandles ()
{
    if (m_hTargetProcess != nullptr) {
        CloseHandle (m_hTargetProcess);
        m_hTargetProcess = nullptr;
    }

    if (m_hWorkerThread != nullptr) {
        CloseHandle (m_hWorkerThread);
        m_hWorkerThread = nullptr;
    }
}

}   // namespace ETWP