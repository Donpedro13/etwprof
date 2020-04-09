#include "ProfilerCommon.hpp"

#include "Log/Logging.hpp"

#include "OS/ETW/ETWConstants.hpp"
#include "OS/ETW/TraceRelogger.hpp"
#include "OS/FileSystem/Utility.hpp"

#include "Utility/Asserts.hpp"

namespace ETWP {

namespace {

bool IsKernelModeAddress (UINT_PTR address)
{
#ifdef ETWP_64BIT
    return address & (1ULL << 63);  // Take advantage of the canonical form
#else
#error Need to implement this to support 32-bit builds!
#endif  // #ifdef ETWP_64BIT
}

bool FilterStackWalkEvent (UCHAR opcode, ProfileFilterData* pFilterData, void* pUserData)
{
    switch (opcode) {
        case ETWConstants::StackWalkOpcode: {
            const ETWConstants::StackWalkDataStub* pData =
                reinterpret_cast<const ETWConstants::StackWalkDataStub*> (pUserData);

            return pData->m_processID == pFilterData->targetPID;
        }

        case ETWConstants::StackKeyKernelOpcode:
        case ETWConstants::StackKeyUserOpcode: {
            const ETWConstants::StackKeyReference* pData =
                reinterpret_cast<const ETWConstants::StackKeyReference*> (pUserData);

            if (pData->m_processID == pFilterData->targetPID) {
                pFilterData->stackKeys.insert (pData->m_key);

                return true;
            } else {
                return false;
            }
        }

        case ETWConstants::StackWalkKeyDeleteOpcode:
        case ETWConstants::StackWalkKeyRundownOpcode: {
            const ETWConstants::StackKeyDefinition* pData =
                reinterpret_cast<const ETWConstants::StackKeyDefinition*> (pUserData);

            if (pFilterData->stackKeys.contains (pData->m_key)) {
                pFilterData->stackKeys.erase (pData->m_key);

                return true;
            } else {
                return false;
            }
        }
    }

    return false;
}

bool FilterThreadEvent (UCHAR opcode, ProfileFilterData* pFilterData, void* pUserData)
{
    const ETWConstants::ThreadDataStub* pData = reinterpret_cast<const ETWConstants::ThreadDataStub*> (pUserData);

    if (pData->m_processID == pFilterData->targetPID) {
        switch (opcode) {
            case ETWConstants::TStartOpcode:
            case ETWConstants::TDCStartOpcode:
                pFilterData->threads.insert (pData->m_threadID);
                break;

            case ETWConstants::TEndOpcode:
                pFilterData->threads.erase (pData->m_threadID);

            case ETWConstants::TDCEndOpcode:
                break;

            default:
                break;
        }

        return true;
    } else {
        return false;
    }
}

bool FilterContextSwitchEvent (UCHAR opcode, ProfileFilterData* pFilterData, void* pUserData)
{
    if (opcode == ETWConstants::ReadyThreadOpcode) {
        const ETWConstants::ReadyThreadDataStub* pData =
            reinterpret_cast<const ETWConstants::ReadyThreadDataStub*> (pUserData);

        return pFilterData->threads.find (pData->m_readyThreadID) != pFilterData->threads.cend ();
    } else if (opcode == ETWConstants::CSwitchOpcode) {
        const ETWConstants::CSwitchDataStub* pData =
            reinterpret_cast<const ETWConstants::CSwitchDataStub*> (pUserData);

        // A thread of interest was involved in a context switch
        return (pFilterData->threads.find (pData->m_newThreadID) != pFilterData->threads.cend () ||
                pFilterData->threads.find (pData->m_oldThreadID) != pFilterData->threads.cend ());
    } else {
        return false;
    }
}

bool FilterProcessEvent (ProfileFilterData* pFilterData, void* pUserData)
{
    const ETWConstants::ProcessDataStub* pData =
        reinterpret_cast<const ETWConstants::ProcessDataStub*> (pUserData);

    return pData->m_processID == pFilterData->targetPID;
}

bool FilterSampledProfileEvent (ProfileFilterData* pFilterData, void* pUserData)
{
    const ETWConstants::SampledProfileDataStub* pData =
        reinterpret_cast<const ETWConstants::SampledProfileDataStub*> (pUserData);

    return pFilterData->threads.find (pData->m_threadID) != pFilterData->threads.cend ();
}

bool FilterImageLoadEvent (ProfileFilterData* pFilterData, void* pUserData)
{
    const ETWConstants::ImageLoadDataStub* pData =
        reinterpret_cast<const ETWConstants::ImageLoadDataStub*> (pUserData);

    return pData->m_processID == pFilterData->targetPID ||
           IsKernelModeAddress (pData->m_imageBase);    // Kernel mode parts of stacks can be interesting, so preserve
                                                        //   kernel module loads (drivers, etc.)
}

bool FilterUserProviderEvent (ProfileFilterData* pFilterData, const GUID& eventGUID)
{
    return pFilterData->userProviders.find ({ eventGUID }) != pFilterData->userProviders.end ();
}

}   // namespace

void FilterEventForProfiling (ITraceEvent* pEvent, TraceRelogger* pRelogger, void* context)
{
    ProfileFilterData* pFilterData = static_cast<ProfileFilterData*> (context);
    EVENT_RECORD* pEventRecord;
    if (FAILED (pEvent->GetEventRecord (&pEventRecord))) {
        Log (LogSeverity::Warning, L"Event dropped in relog (GetEventRecord failed)!");

        return;
    }

    EVENT_HEADER* pHeader = &pEventRecord->EventHeader;
    if (pHeader->ProviderId == StackWalkGuid) {
        if (FilterStackWalkEvent (pHeader->EventDescriptor.Opcode, pFilterData, pEventRecord->UserData)) {
            std::wstring errorMsg;
            if (!pRelogger->Inject (pEvent, &errorMsg))
                Log (LogSeverity::Warning, L"Injecting event failed: " + errorMsg);

            return;
        }
    } else if (pHeader->ProviderId == PerfInfoGuid &&
               pHeader->EventDescriptor.Opcode == ETWConstants::SampledProfileOpcode)
    {
        if (FilterSampledProfileEvent (pFilterData, pEventRecord->UserData)) {
            std::wstring errorMsg;
            if (!pRelogger->Inject (pEvent, &errorMsg))
                Log (LogSeverity::Warning, L"Injecting event failed: " + errorMsg);

            return;
        }
    } else if (pHeader->ProviderId == ThreadGuid) {
        UCHAR opcode = pHeader->EventDescriptor.Opcode;
        if (opcode == ETWConstants::TStartOpcode   ||
            opcode == ETWConstants::TDCStartOpcode ||
            opcode == ETWConstants::TEndOpcode     ||
            opcode == ETWConstants::TDCEndOpcode)
        {
            if (FilterThreadEvent (opcode, pFilterData, pEventRecord->UserData)) {
                std::wstring errorMsg;
                if (!pRelogger->Inject (pEvent, &errorMsg))
                    Log (LogSeverity::Warning, L"Injecting event failed: " + errorMsg);

                return;
            }
        } else if (pFilterData->cswitch) {
            if (opcode == ETWConstants::ReadyThreadOpcode || opcode == ETWConstants::CSwitchOpcode) {
                if (FilterContextSwitchEvent (opcode, pFilterData, pEventRecord->UserData)) {
                    std::wstring errorMsg;
                    if (!pRelogger->Inject (pEvent, &errorMsg))
                        Log (LogSeverity::Warning, L"Injecting event failed: " + errorMsg);

                    return;
                }
            }
        }
    } else if (pHeader->ProviderId == ProcessGuid) {
        if (FilterProcessEvent (pFilterData, pEventRecord->UserData)) {
            std::wstring errorMsg;
            if (!pRelogger->Inject (pEvent, &errorMsg))
                Log (LogSeverity::Warning, L"Injecting event failed: " + errorMsg);

            return;
        }
    }  else if (pHeader->ProviderId == ImageLoadGuid) {
        if (FilterImageLoadEvent (pFilterData, pEventRecord->UserData)) {
            std::wstring errorMsg;
            if (!pRelogger->Inject (pEvent, &errorMsg))
                Log (LogSeverity::Warning, L"Injecting event failed: " + errorMsg);

            return;
        }
    } else if (pHeader->ProviderId == EventTraceEventGuid) {
        std::wstring errorMsg;
        if (!pRelogger->Inject (pEvent, &errorMsg))
            Log (LogSeverity::Warning, L"Injecting event failed: " + errorMsg);

        return;
    } else if (pHeader->ProcessId == pFilterData->targetPID) {
        if (FilterUserProviderEvent (pFilterData, pEventRecord->EventHeader.ProviderId)) {
            std::wstring errorMsg;
            if (!pRelogger->Inject (pEvent, &errorMsg))
                Log (LogSeverity::Warning, L"Injecting event failed: " + errorMsg);

            return;
        }
    }
}

bool MergeTrace (const std::wstring& inputETLPath,
                 DWORD flags,
                 const std::wstring& outputETLPath,
                 std::wstring* pErrorOut)
{
    if (!PathExists (inputETLPath)) {
        *pErrorOut = L"Input ETL file does not exist!";

        return false;
    }

    if (!PathValid (outputETLPath)) {
        *pErrorOut = L"Output ETL file path is invalid!";

        return false;
    }

    using CreateMergedTraceFile_t = std::add_pointer_t<ULONG WINAPI (_In_  LPCWSTR wszMergedFileName,
                                                                     _In_reads_ (cTraceFileNames) LPCWSTR wszTraceFileNames[],
                                                                     _In_  ULONG cTraceFileNames,
                                                                     _In_  DWORD dwExtendedDataFlags)>;
    
    // Merge debug info using kerneltracecontrol.dll
#ifndef ETWP_DEBUG
    // Do not display error dialog if DLL loading fails
    UINT prevErrorMode = SetErrorMode (SEM_FAILCRITICALERRORS);
    OnExit errorModeRestorer ([&prevErrorMode] () { SetErrorMode (prevErrorMode); });
#endif  // #ifndef ETWP_DEBUG

    HMODULE kernelTraceControlDLL = LoadLibraryW (L"kerneltracecontrol.dll");
    if (kernelTraceControlDLL == nullptr) {
        *pErrorOut = L"Unable to load kerneltracecontrol.dll!";

        return false;
    }

    OnExit ktcDLLReleaser ([&kernelTraceControlDLL]() {
        FreeLibrary (kernelTraceControlDLL);
    });

    CreateMergedTraceFile_t pMergeFunction =
        reinterpret_cast<CreateMergedTraceFile_t> (GetProcAddress (kernelTraceControlDLL, "CreateMergedTraceFile"));

    if (pMergeFunction == nullptr) {
        *pErrorOut = L"Cannot locate CreateMergedTraceFile in kerneltracecontrol.dll!";

        return false;
    }

    LPCWSTR filenames[] = { inputETLPath.c_str () };
    const ULONG result = pMergeFunction (outputETLPath.c_str (),
                                         filenames,
                                         1,
                                         flags);

    if (ETWP_ERROR (result != ERROR_SUCCESS)) {
        *pErrorOut = L"CreateMergedTraceFile returned error!";

        return false;
    }

    return true;
}

}   // namespace ETWP