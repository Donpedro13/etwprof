#include "ETWProfiler.hpp"

#include <process.h>
#include <string>
#include <unordered_set>
#include <vector>

#include "Log/Logging.hpp"

#include "OS/ETW/ETWConstants.hpp"
#include "OS/ETW/ETWSessionCommon.hpp"
#include "OS/ETW/TraceRelogger.hpp"
#include "OS/ETW/CombinedETWSession.hpp"

#include "OS/FileSystem/Utility.hpp"

#include "OS/Process/ProcessRef.hpp"
#include "OS/Process/Utility.hpp"

#include "OS/Synchronization/LockableGuard.hpp"

#include "ProfilerCommon.hpp"

#include "Utility/Asserts.hpp"
#include "Utility/GUID.hpp"
#include "Utility/OnExit.hpp"

namespace ETWP {

ETWProfiler::ETWProfiler (const std::wstring& outputPath,
                          const std::vector<PID>& targetPIDs,
                          const ProfileRate& samplingRate,
                          IETWBasedProfiler::Flags options,
                          FinishCriterion finishCriterion):
    m_lock (),
    m_resultLock (),
    m_hWorkerThread (nullptr),
    m_ETWSession (nullptr),
    m_originalTargets (),
    m_userProviders (),
    m_samplingRate (samplingRate),
    m_options (static_cast<Options> (options)),
    m_finishCriterion (finishCriterion),
    m_outputPath (outputPath),
    m_state (State::Unstarted),
    m_errorFromWorkerThread ()
{
    if (!PathValid (m_outputPath))
        throw InitException (L"Output ETL path is invalid!");

    if (targetPIDs.empty ())
        throw InitException (L"No targets to profile!");

    // Open HANDLEs to the target processes, so we can wait for them
    for (auto pid : targetPIDs) {
        try {
            ProcessRef target (pid, ProcessRef::AccessOptions::Synchronize);
            m_originalTargets.Add (std::move (target));
        } catch (ProcessRef::InitException& e) {            
            throw InitException (L"Unable to open HANDLE for target process with pid " +
                                 std::to_wstring (pid) + L"(" + e.GetMsg () + L")" + L"!");
        }
    }

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

    m_ETWSession.reset (new CombinedETWSession (GenerateETWSessionName (), kProfilingFlags));

    Log (LogSeverity::Debug, L"ETW session name of profiler will be: " + m_ETWSession->GetName ());
}

ETWProfiler::~ETWProfiler ()
{
    LockableGuard lockGuard (&m_lock);

    State        dummy1;
    std::wstring dummy2;
    if (!IsFinished (&dummy1, &dummy2))
        Stop ();

    CloseHandles ();
}

bool ETWProfiler::Start (std::wstring* pErrorOut)
{
    LockableGuard lockGuard (&m_lock);

    if (ETWP_ERROR (IsProfiling ()))
        return true;

    ETWP_ASSERT (pErrorOut != nullptr);
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

    return true;
}

void ETWProfiler::Stop ()
{
    LockableGuard lockGuard (&m_lock);

    SetState (State::Stopped);

    StopImpl ();
}

void ETWProfiler::Abort ()
{
    LockableGuard lockGuard (&m_lock);

    SetState (State::Aborted);

    StopImpl ();
}

bool ETWProfiler::IsFinished (State* pResultOut, std::wstring* pErrorOut)
{
    LockableGuard lockGuard (&m_lock);

    if (!IsProfiling ()) {
        if (GetState () == IProfiler::State::Unstarted)
            return false;   // We're not profiling *yet*

        WaitForProfilerThread ();

        LockableGuard resultLockGuard (&m_resultLock);

        *pResultOut = m_state;

        if (m_state == IProfiler::State::Error)
            *pErrorOut = m_errorFromWorkerThread;

        return true;
    }

    // If the profiler thread is still running, poll the target process(es) to see if they have finished yet
    if (m_finishCriterion == FinishCriterion::AllTargetsFinished ?
            HaveAllTargetProcessesExited () : HaveOriginalTargetProcessesExited ())
    {
        SetState (State::Finished);

        StopImpl ();    // Stop the kernel session, wait for the thread to finish

		LockableGuard resultLockGuard (&m_resultLock);

        ETWP_ASSERT (m_state != State::Running && m_state != State::Unstarted);

		*pResultOut = m_state;

		if (m_state == State::Error)
			*pErrorOut = m_errorFromWorkerThread;

        return true;
    }

    return false;
}

bool ETWProfiler::EnableProvider (const IETWBasedProfiler::ProviderInfo& providerInfo)
{
    LockableGuard lockGuard (&m_lock);

    if (ETWP_ERROR (IsProfiling ()))
        return false;

    m_userProviders.push_back (providerInfo);

    return true;
}

uint32_t ETWProfiler::GetNumberOfProfiledProcesses ()
{
    // These two members have their own lock
    return m_originalTargets.GetWaitingSize () + m_additionalTargets.GetWaitingSize ();
}

unsigned int ETWProfiler::ProfileHelper (void* instance)
{
    ETWProfiler* pInstance = static_cast<ETWProfiler*> (instance);

    pInstance->Profile ();

    _endthreadex (0);
    
    return 0;   // Never reached, but the compiler isn't aware
}

void ETWProfiler::ProcessStarted (PID pid, PID /*parentPID*/)
{
    try {
        ProcessRef target (pid, ProcessRef::AccessOptions::Synchronize);
        m_additionalTargets.Add (std::move (target));
    } catch (const ProcessRef::InitException&) {
        // This can happen, if a to-be profiled process exits very quickly after starting. This causes no problem
        //  for us, as: - events are still collected for this process (ETW events are a "stream", if you will)
        //              - the process not being in this container (of HANDLEs) would cause not detecting when it exits;
        //                  but we could not add it in the first place because it already exited
    }
}

void ETWProfiler::ProcessEnded (PID pid, PID /*parentPID*/)
{
    m_additionalTargets.Delete (pid);
}

void ETWProfiler::StopImpl ()
{
    if (ETWP_ERROR (!m_ETWSession->Stop ())) {
        // We will probably leak an ETW session, which is bad :/
        Log (LogSeverity::Warning, L"Unable to stop ETW session: " + m_ETWSession->GetName () + L"!");
    }

    WaitForProfilerThread ();
}

void ETWProfiler::Profile ()
{
    LockableGuard lockGuard (&m_lock);

    ETWP_DEBUG_ONLY (OnExit stateChecker ([this]() { ETWP_ASSERT (GetState () != State::Running); }));

    std::unordered_set<DWORD> targetPIDs;
    for (const auto& [pid, _] : m_originalTargets)
        targetPIDs.insert (pid);

    // Create copy of data needed by the filtering relogger callback, so it can run lockless
    ProfileFilterData filterData = { { },
                                     { m_userProviders.begin (), m_userProviders.end () },
                                     {},
                                     std::move (targetPIDs),
                                     bool (m_options & RecordCSwitches),
                                     bool (m_options & ProfileChildren) };

    // These will be used later, we create a copy as well (so no locking will be required)
    std::wstring outputPath = m_outputPath;
    std::wstring rawOutputPath = m_outputPath + L".raw.etl";
    bool debug = m_options & Debug;
    bool compress = m_options & Compress;

    try {
        // Create and set up the relogger before starting the kernel logger. This way we minimize the time between
        // relogging (~consuming) events, and the kernel logger emitting events into buffers
        ProfileEventFilter eventFilter (filterData);
        eventFilter.Attach (this);
        TraceRelogger filteringRelogger (&eventFilter,
                                         rawOutputPath,
                                         m_options & Compress);

        if (ETWP_ERROR (!m_ETWSession->Start ())) {
            SetErrorFromWorkerThread (L"Unable to start ETW session!");

            return;
        }

        OnExit etwSessionDestroyer ([&]() {
            // We could exit ETWProfiler::Profile when the lock is unowned, so we need this
            LockableGuard guard (&m_lock);

            m_ETWSession->Stop ();
        });

        // We need stack traces for: - sampled profile
        //                           - ready thread and context switch (if we collect context switch data)
        std::vector<CLASSIC_EVENT_ID> eventIDArray;
        eventIDArray.push_back ({ PerfInfoGuid, ETWConstants::SampledProfileOpcode, {} });

        if (m_options & RecordCSwitches) {
            eventIDArray.push_back ({ ThreadGuid, ETWConstants::ReadyThreadOpcode, {} });
            eventIDArray.push_back ({ ThreadGuid, ETWConstants::CSwitchOpcode,     {} });
        }

        if (eventIDArray.size () > 0 &&
            ETWP_ERROR (TraceSetInformation (m_ETWSession->GetNativeHandle (),
                                             TraceStackTracingInfo,
                                             &eventIDArray[0],
                                             static_cast<DWORD> (eventIDArray.size ()) *
                                                sizeof (CLASSIC_EVENT_ID)) != ERROR_SUCCESS))
        {
            SetErrorFromWorkerThread (L"Unable to request stack traces for ETW session!");

            return;
        }

        for (auto&& providerInfo : filterData.userProviders) {
            if (ETWP_ERROR (!m_ETWSession->EnableProvider (&providerInfo.providerID,
                                                             providerInfo.stack,
                                                             providerInfo.level,
                                                             providerInfo.flags)))
            {
                std::wstring guidString = L"UNKNOWN PROVIDER";
                GUIDToString (providerInfo.providerID, &guidString);
                SetErrorFromWorkerThread (L"Failed to enable user provider: " + guidString);

                return;
            }
        }

        if (m_options & StackCache) {
            if (ETWP_ERROR (!EnableStackCachingForSession (m_ETWSession->GetNativeHandle (),
                                                           40 * 1'024 * 1'024, 40'961)))
            {
                SetErrorFromWorkerThread (L"Failed to enable stack caching for ETW session!");

				return;
            }
        }

        std::wstring errorMsg;
        if (ETWP_ERROR (!filteringRelogger.AddRealTimeSession (*m_ETWSession.get (), &errorMsg))) {
            SetErrorFromWorkerThread (L"Unable to add ETW session to filtering relogger: " + errorMsg);

            return;
        }

        // Note: we unlock the lock, so the relogging process can run lock free
        lockGuard.Unlock ();

        SetState (State::Running);

        // This will call back FilterEventForProfiling
        if (ETWP_ERROR (!filteringRelogger.StartRelogging (&errorMsg))) {
            SetErrorFromWorkerThread (L"Unable to start relogger: " + errorMsg);

            return;
        }

        // At this point, the session must already be stopped, so no need for this
        etwSessionDestroyer.Deactivate ();
    } catch (const TraceRelogger::InitException& e) {
        SetErrorFromWorkerThread (L"Unable to construct filtering relogger: " + e.GetMsg ());

        return;
    }

    OnExit rawETLDeleter ([&rawOutputPath]() {
        // Delete temporary (unmerged) file
        if (ETWP_ERROR (!FileDelete (rawOutputPath)))
            Log (LogSeverity::Debug, L"Unable to delete raw ETL file!");
    });

    if (debug)
        rawETLDeleter.Deactivate ();

	// The kernel session stopped, possible causes:
	// a.) the target process(es) exited (detected by IsFinished)
	// b.) profiling was stopped manually (Stop or Abort was called)
    // c.) the kernel session was stopped externally (outside of etwprof)
    
    // Find out if we are up against case "c"
	if (GetState () == State::Running)
        SetState (State::Aborted);

    const State currentState = GetState ();
    ETWP_ASSERT (currentState != State::Running && currentState != State::Unstarted);

    if (currentState == IProfiler::State::Aborted)
        return;

    DWORD mergeFlags = EVENT_TRACE_MERGE_EXTENDED_DATA_IMAGEID | EVENT_TRACE_MERGE_EXTENDED_DATA_EVENT_METADATA;
    if (compress)
        mergeFlags |= EVENT_TRACE_MERGE_EXTENDED_DATA_COMPRESS_TRACE;

    std::wstring mergeErrorMsg;
    if (ETWP_ERROR (!MergeTrace (rawOutputPath, mergeFlags, outputPath, &mergeErrorMsg))) {
        SetErrorFromWorkerThread (L"Debug info merge failed: " + mergeErrorMsg);

        return;
    }
}

bool ETWProfiler::IsProfiling ()
{
	LockableGuard resultLockGuard (&m_resultLock);

	return m_state == State::Running;
}

std::wstring ETWProfiler::GenerateETWSessionName ()
{
    DWORD ownPID = GetProcessId (GetCurrentProcess ());

    return L"ETWProf_Session_" + std::to_wstring (ownPID);
}

void ETWProfiler::CloseHandles ()
{
    if (m_hWorkerThread != nullptr) {
        CloseHandle (m_hWorkerThread);
        m_hWorkerThread = nullptr;
    }
}

void ETWProfiler::SetErrorFromWorkerThread (const std::wstring& message)
{
	LockableGuard resultLockGuard (&m_resultLock);

	m_state = State::Error;
	m_errorFromWorkerThread = message;
}

void ETWProfiler::SetState (State newState)
{
	LockableGuard resultLockGuard (&m_resultLock);

	m_state = newState;
}

IProfiler::State ETWProfiler::GetState ()
{
	LockableGuard resultLockGuard (&m_resultLock);

    State result = m_state;

    return result;
}

bool ETWProfiler::HaveAllTargetProcessesExited ()
{
    return HaveOriginalTargetProcessesExited () && m_additionalTargets.IsAllFinished ();
}

bool ETWProfiler::HaveOriginalTargetProcessesExited ()
{
    return m_originalTargets.IsAllFinished ();
}

void ETWProfiler::WaitForProfilerThread ()
{
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
}

}   // namespace ETWP