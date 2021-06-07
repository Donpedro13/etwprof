#include "Application.hpp"

#include <algorithm>
#include <cstdlib>

#ifdef ETWP_DEBUG
#include <crtdbg.h>
#endif

#include "Error.hpp"
#include "ProgressFeedback.hpp"

#include "Log/Logging.hpp"

#include "OS/FileSystem/Utility.hpp"
#include "OS/Process/Minidump.hpp"
#include "OS/Process/Utility.hpp"
#include "OS/Stream/GlobalStreams.hpp"
#include "OS/Version/WinVersion.hpp"

#include "Profiler/ETLReloggerProfiler.hpp"
#include "Profiler/ETWProfiler.hpp"

#include "Utility/Asserts.hpp"
#include "Utility/OnExit.hpp"

namespace ETWP {

namespace {

#ifdef ETWP_DEBUG
void InitMemLeakDetection ()
{
    _CrtSetDbgFlag (_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);

}
#endif  // #ifdef ETWP_DEBUG

bool InitCOM ()
{
    if (FAILED (CoInitializeEx (nullptr, COINIT_MULTITHREADED)))
        return false;
    else
        return true;
}

void UninitCOM ()
{
    CoUninitialize ();
}

}   // namespace

Application& Application::Instance ()
{
    static Application instance;

    return instance;
}

bool Application::Init (const ArgumentVector& arguments)
{
#ifdef ETWP_DEBUG
    InitMemLeakDetection ();
#endif

    if (!HandleArguments (arguments))
        return false;
    
    if (!InitCOM ())
        return false;

    return true;
}

Application::Application ()
{
    
}

Application::~Application ()
{
    UninitCOM ();
}

BOOL WINAPI Application::CtrlHandler (DWORD fdwCtrlType)
{
    switch (fdwCtrlType) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
            Log (LogSeverity::Debug, L"CTRL+C/CTRL+Break detected. Stopping profiler.");
            Application::Instance ().m_pProfiler->Stop ();
            break;
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            Log (LogSeverity::Debug,
                 L"Close/shutdown/logoff detected. Aborting profiler.");
            Application::Instance ().m_pProfiler->Abort ();

            return FALSE;
            break;
        default:
            ETWP_DEBUG_BREAK_STR (L"Invalid value in " __FUNCTIONW__ L"!");
            break;
    }

    return TRUE;
}

int Application::Run ()
{
    if (m_args.noAction)    // "Hidden" option for testing command line arguments
        return 0;

    if (m_args.verbose)
        SetMinLogSeverity (LogSeverity::Info);

    if (m_args.debug)
        SetMinLogSeverity (LogSeverity::Debug);

    if (!m_args.noLogo)
        PrintLogo ();

    if (m_args.help) {
        PrintUsage ();

        return 0;
    }

    // Check Windows version
    if (!CheckWinVersion ())
        return int (GlobalErrorCodes::InitializationError);

    if (m_args.profile) {
        if (!DoProfile ())
            return int (GlobalErrorCodes::ProfilingInitializationError);
    }

    return 0;
}

bool Application::CheckWinVersion () const
{
    constexpr BaseWinVersion kMinVersion = BaseWinVersion::Win7;
    constexpr wchar_t kwinVersionError[] = L"Invalid Windows version! "
        L"Windows 7 or later is required to run this program. ";
    constexpr wchar_t kwinVersionTypeError[] = L"Invalid Windows version! "
        L"Server versions are unsupported at this time.";
    
    if (GetWinVersion () < kMinVersion) {
        Log (LogSeverity::Error, kwinVersionError);

        return false;
    }
    
    if (GetWinVersionType () == WinVersionType::Server) {
        Log (LogSeverity::Error, kwinVersionTypeError);

        return false;
    }
    
    return true;
}

void Application::PrintUsage () const
{
    constexpr wchar_t kUsageString[] =
        LR"(etwprof

  Usage:
    etwprof profile --target=<PID_or_name> (--output=<file_path> | --outdir=<dir_path>) [--mdump [--mflags]] [--compress=<mode>] [--enable=<args>] [--cswitch] [--rate=<profile_rate>] [--nologo] [--verbose] [--debug] [--scache]
    etwprof profile --emulate=<ETL_path> --target=<PID> (--output=<file_path> | --outdir=<dir_path>) [--compress=<mode>] [--enable=<args>] [--cswitch] [--nologo] [--verbose] [--debug]
    etwprof --help

  Options:
    -h --help        Show this screen
    -v --verbose     Enable verbose output
    -t --target=<t>  Target (PID or exe name)
    -o --output=<o>  Output file path
    -d --debug       Turn on debug mode (even more verbose logging, preserve intermediate files, etc.)
    -m --mdump       Write a minidump of the target process at the start of profiling
    --mflags=<f>     Dump type flags for minidump creation in hex format [default: 0x0 aka. MiniDumpNormal]
    --outdir=<od>    Output directory path
    --nologo         Do not print logo
    --rate=<r>       Sampling rate (in Hz) [default: use current global rate]
    --compress=<c>   Compression method used on output file ("off", "etw", or "7z") [default: "etw"]
    --enable=<args>  Format: (<GUID>|<RegisteredName>|*<Name>)[:KeywordBitmask[:MaxLevel['stack']]][+...]
    --scache         Enable ETW stack caching
    --cswitch        Collect context switch events as well (beta feature)
    --emulate=<f>    Debugging feature. Do not start a real time ETW session, use an already existing ETL file as input
)";

    COut () << kUsageString;
}

void Application::PrintLogo () const
{
    COut () << ColorReset << L"etwprof\tVersion "
        ETWP_EXPAND_AND_STRINGIFY_WIDE (ETWP_MAJOR_VERSION) L"."
        ETWP_EXPAND_AND_STRINGIFY_WIDE (ETWP_MINOR_VERSION) <<
#if defined ETWP_PATCH_VERSION && ETWP_PATCH_VERSION > 0
        L"." ETWP_EXPAND_AND_STRINGIFY_WIDE (ETWP_PATCH_VERSION)
#endif

#ifdef _WIN64
        L" (x64)"
#else
        L" (x86)"
#endif  // #ifdef _WIN64

#ifdef ETWP_DEBUG
        << FgColorYellow << L" [" << FgColorRed << L"DEBUG BUILD" <<
            FgColorYellow << L"]" << ColorReset
#endif  // #ifdef ETWP_DEBUG
        << L" - " << FgColorWhite << L"https://github.com/Donpedro13/etwprof" << ColorReset << Endl;

    COut () << L"Copyright (c) 2021 " << FgColorWhite << L"P\u00E9ter \u00C9sik" <<
        ColorReset << L" - " << FgColorWhite << L"https://peteronprogramming.wordpress.com/" << ColorReset << Endl;

    COut () << L"See LICENSE.txt for license details" << Endl << Endl;
}

bool Application::DoProfile ()
{
    ScopeLogger logger (LogSeverity::Debug,
                        L"Starting profiling command",
                        L"Stopping profiling command");

    ProcessList::Process target;
    HANDLE hTargetProcess = INVALID_HANDLE_VALUE;
    if (!m_args.emulate) {
        if (!GetTarget (&target))
            return false;

        Log (LogSeverity::Info, L"Found valid target: " + target.m_name + L" (" + std::to_wstring (target.m_PID) +
             L")");

        // We open a HANDLE to the target process, lest it finishes and we profile an unintended process (this way Windows
        //   will not reassign the PID of the process, if it happens to exit while we are in this function)
        // We might use this HANDLE later to write a minidump, hence PROCESS_QUERY_INFORMATION | PROCESS_VM_READ
        hTargetProcess = OpenProcess (PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, target.m_PID);
    }

    OnExit processHandleCloser ([&hTargetProcess] () {
        if (hTargetProcess != INVALID_HANDLE_VALUE)
            CloseHandle (hTargetProcess);
    });

    std::wstring finalOutputPath = m_args.output;
    // If the specified out path is a folder, we need to generate an appropriate file name, and append it to the output
    //   directory
    if (!m_args.outputIsFile)
        finalOutputPath += GenerateDefaultOutputName (target, m_args.compressionMode);

    // If 7z compression is requested, we need to change the profiler output path
    std::wstring profilerOutputPath = finalOutputPath;
    if (m_args.compressionMode == ApplicationArguments::CompressionMode::SevenZip)
        profilerOutputPath = PathReplaceExtension (finalOutputPath, L".etl");

    Log (LogSeverity::Info, L"Output file path is " + finalOutputPath);
    if (finalOutputPath != profilerOutputPath)
        Log (LogSeverity::Info, L"Profiler output file path is " + profilerOutputPath);

    IETWBasedProfiler::Flags options = IETWBasedProfiler::Default;
    if (m_args.cswitch)
        options |= IETWBasedProfiler::RecordCSwitches;

    if (m_args.compressionMode == ApplicationArguments::CompressionMode::ETW)
        options |= IETWBasedProfiler::Compress;

    if (m_args.debug)
        options |= IETWBasedProfiler::Debug;

	if (m_args.stackCache)
		options |= IETWBasedProfiler::StackCache;

    if (m_args.emulate) {
        Log (LogSeverity::Info, L"Constructing \"emulate mode\" profiler");

        try {
            m_pProfiler.reset (new ETLReloggerProfiler (m_args.inputETLPath,
                                                        profilerOutputPath,
                                                        m_args.targetPID,
                                                        options));
        } catch (const IProfiler::InitException& e) {
            Log (LogSeverity::Error, L"Unable to construct relogger profiler object: " + e.GetMsg ());

            return false;
        }
    } else {
        if (ETWP_ERROR (!AddProfilePrivilegeToProcessToken ())) {
            Log (LogSeverity::Error, L"Unable to add \"SeSystemProfilePrivilege\" to the process' token! Contact your "
                 L"IT department!");

            return false;
        }

        try {
            m_pProfiler.reset (new ETWProfiler (profilerOutputPath,
                                                target.m_PID,
                                                ConvertSamplingRateFromHz (m_args.samplingRate),
                                                options));
        } catch (const IProfiler::InitException& e) {
            Log (LogSeverity::Error, L"Unable to construct profiler object: " + e.GetMsg ());

            return false;
        }
    }

    // Enable user providers
    for (auto&& provInfo : m_args.userProviderInfos)
        m_pProfiler->EnableProvider ({provInfo.guid, provInfo.stack, provInfo.maxLevel, provInfo.keywordBitmask });

    // Before starting profiling, write a minidump, if needed
    if (m_args.minidump && ETWP_VERIFY (!m_args.emulate)) {
        std::wstring dumpPath = PathReplaceExtension (finalOutputPath, L".dmp");
        Log (LogSeverity::Info, L"Minidump path is " + dumpPath);

        std::wstring error;
        if (ETWP_ERROR (!CreateMinidump (dumpPath, hTargetProcess, target.m_PID, m_args.minidumpFlags, &error)))
            Log (LogSeverity::Warning, L"Unable to create minidump: " + error);
    }
    
    // Set up a console control handler for handling interruptions
    if (ETWP_ERROR (SetConsoleCtrlHandler (&Application::CtrlHandler, TRUE) == FALSE)) {
        Log (LogSeverity::Error, L"Unable to install Console Control Handler!");

        return false;
    }

    OnExit consoleCtrlHandlerRemover ([] () {
        if (SetConsoleCtrlHandler (&Application::CtrlHandler, FALSE) == FALSE)
            Log (LogSeverity::Warning, L"Unable to remove Console Control Handler!");
    });

    std::wstring startErrorMsg;
    if (!m_pProfiler->Start (&startErrorMsg)) {
        Log (LogSeverity::Error, L"Starting profiler failed: " + startErrorMsg);

        return false;
    }

    // We can remove this, when we finally figure out what we are doing wrong with context switch recording...
    if (m_args.cswitch) {
        Log (LogSeverity::Warning, L"Context switch recording is a BETA feature, and has KNOWN ISSUES. Keep this in mind "
                                   L"when analyzing the resulting trace. See the \"Known Issues\" page on the GitHub repo "
                                   L"for details.");
    }

    ProgressFeedback::Style feebackStyle = COut ().GetType () == ConsoleOStream::Type::Console ?
                                            ProgressFeedback::Style::Animated : ProgressFeedback::Style::Static;
    ProgressFeedback feedback (L"Profiling",
                               target.m_name + L" (" + std::to_wstring (target.m_PID) + L")",
                               feebackStyle,
                               ProgressFeedback::State::Running);
    
    IProfiler::State state;
    std::wstring profilingErrorMsg;
    while (!m_pProfiler->IsFinished (&state, &profilingErrorMsg)) {
        feedback.PrintProgress ();

        constexpr DWORD kProgressFrequencyMs = 500;
        Sleep (kProgressFrequencyMs);
    }

    consoleCtrlHandlerRemover.Trigger ();

    // Profiling exited; update feedback accordingly
    switch (state) {
        case IProfiler::State::Finished:
        case IProfiler::State::Stopped:
            feedback.SetState (ProgressFeedback::State::Finished);
            break;
        case IProfiler::State::Aborted:
            feedback.SetState (ProgressFeedback::State::Aborted);
            break;
        case IProfiler::State::Error:
            feedback.SetState (ProgressFeedback::State::Error);
            break;
        default:
            // This shouldn't happen
            ETWP_DEBUG_BREAK_STR (L"Invalid profiler status");
            break;
    }

    feedback.PrintProgressLine ();

    if (state == IProfiler::State::Error) {
        Log (LogSeverity::Error, L"Profiling finished with an error: " + profilingErrorMsg);

        return false;
    }

    // 7z-compress output file, if needed
    if (m_args.compressionMode == ApplicationArguments::CompressionMode::SevenZip) {
        if (PathExists (finalOutputPath)) {
            Log (LogSeverity::Info, L"Deleting file " + finalOutputPath + L" as it already exists");

            ETWP_VERIFY (FileDelete (finalOutputPath));
        }

        WCHAR exePath[MAX_PATH];
        if (GetModuleFileNameW (nullptr, exePath, MAX_PATH) != 0) {
            WCHAR exeDrive[_MAX_DRIVE];
            WCHAR exeDir[_MAX_DIR];
            if (_wsplitpath_s (exePath,
                               exeDrive,
                               _MAX_DRIVE,
                               exeDir,
                               _MAX_DIR,
                               nullptr,
                               0,
                               nullptr,
                               0) == 0)
            {
                const std::wstring sevenZipPath = exeDrive +
                    std::wstring(exeDir) + L"7zr.exe";

                std::wstring commandLine = L"a -m0=lzma2 -mx=4 " +
                    std::wstring (L"\"") + finalOutputPath + L"\" \"" +
                    profilerOutputPath + L"\"";

                
                COut () << ColorReset << L"Launching 7zr for compression..." << Endl;

                DWORD status;
                if (ETWP_ERROR (!CreateProcessSynchronous (sevenZipPath, commandLine, &status))) {
                    Log (LogSeverity::Error, L"Unable to start 7z for compression!");

                    return false;
                }

                if (ETWP_ERROR (status != 0)) {
                    Log (LogSeverity::Error, L"7z returned error while compressing trace!");

                    return false;
                }

                if (!m_args.debug && ETWP_ERROR (!FileDelete (profilerOutputPath)))
                    Log (LogSeverity::Warning, L"Unable to delete profiler output path!");

                COut () << ColorReset << L"7z compression done" << Endl;
            }
        }
    }

    if (state == IProfiler::State::Finished)
        Log (LogSeverity::Info, L"Profiling finished because the target process exited!");
    else
        Log (LogSeverity::Info, L"Profiling finished because it was stopped manually!");
    
    return true;
}

bool Application::GetTarget (ProcessList::Process* pTargetOut) const
{
    try {
        ProcessList processList;

        if (m_args.targetIsPID) {
            if (processList.Contains (m_args.targetPID)) {
                Log (LogSeverity::Info, L"PID was found in the process list");
                ETWP_VERIFY (processList.GetName (m_args.targetPID, &pTargetOut->m_name));
                pTargetOut->m_PID = m_args.targetPID;
            } else {
                Log (LogSeverity::Error, L"Given PID was not found in the running processes list!");

                return false;
            }
        } else {
            uint32_t count = processList.GetCount (m_args.targetName);
            Log (LogSeverity::Info, L"Given process name was found in the running processes list " +
                    std::to_wstring (count) +
                    L" times");

            if (count == 0) {
                Log (LogSeverity::Error, L"Given process name was not found in the running processes list!");

                return false;
            } else if (count > 1) {
                Log (LogSeverity::Error, L"Given process name was found more than once in the running processes list!");

                return false;
            } else {
                pTargetOut->m_name = m_args.targetName;
                pTargetOut->m_PID = processList.GetPID (m_args.targetName);

                ETWP_ASSERT (pTargetOut->m_PID != 0);

                Log (LogSeverity::Debug, L"PID for process name is " + std::to_wstring (pTargetOut->m_PID));
            }
        }
    } catch (const ProcessList::InitException& /*e*/) {
        Log (LogSeverity::Error, L"Unable to get running processes list!");

        return 0;
    }

    // If we got here, we should have a valid PID
    ETWP_ASSERT (pTargetOut->m_PID != 0);

    return true;
}


std::wstring Application::GenerateDefaultOutputName (const ProcessList::Process& process,
                                                     ApplicationArguments::CompressionMode compressionMode) const
{
    // Format: <AppName>[PID]-<DateTime>(.etl|.7z)
    std::wstring result = process.m_name + L"[" + std::to_wstring (process.m_PID) + L"]-";

    SYSTEMTIME time;
    GetSystemTime (&time);

    constexpr size_t kMaxDateTimeLength = 256;
    wchar_t dateTimeString[kMaxDateTimeLength];
    if (ETWP_ERROR (swprintf_s (dateTimeString,
                                kMaxDateTimeLength,
                                L"%04u-%02u-%02u(%02u-%02u-%02u)",
                                time.wYear,
                                time.wMonth,
                                time.wDay,
                                 time.wHour,
                                time.wMinute,
                                time.wSecond) == -1))
    {
        wcscpy_s (dateTimeString, kMaxDateTimeLength, L"UNKNOWN_DATETIME");
    }

    result += dateTimeString;

    // Replace all dots with underscores for easier readability
    std::replace (result.begin (), result.end (), L'.', L'_');

    result += compressionMode == ApplicationArguments::CompressionMode::SevenZip ? L".7z" : L".etl";

    ETWP_ASSERT (PathValid (result));

    return result;
}

bool Application::HandleArguments (const Application::ArgumentVector& arguments)
{
    // Note: There is a chicken and egg problem here. --debug and --verbose are
    //   command line arguments, but debug or verbose mode can be 
    //   useful for tracing argument handling in certain situations. There are
    //   three possible solutions for this problem:
    //     1.) compile with Debug as default severity in debug builds
    //     2.) cherry-pick --debug and --verbose before ParseArguments
    //     3.) use fix verbosity levels while processing the arguments
    //
    // Option 1 gets really annoying quickly, as just because you are running
    //   a debug build, it doesn't mean you want to see the most verbose output
    //   all the time. Option 2 felt really hacky, so I went with option 3,
    //   specifically, overriding verbosity to the maximum level temporarily
    //   during argument handling if the build configuration is debug. This
    //   seems to be a good compromise, as argument error diagnostics are
    //   already quite nice due to our custom processing logic, but in the
    //   (hopefully) rare case of a puzzling argument errors, you can still
    //   try your command line with a debug build, and get more verbose output.

#ifdef ETWP_DEBUG
    LogSeverity prevLogSeverity = GetMinLogSeverity ();
    SetMinLogSeverity (LogSeverity::MostVerbose);

    OnExit logSeverityRestorer ([&prevLogSeverity] () { SetMinLogSeverity (prevLogSeverity); });
#endif  // #ifdef ETWP_DEBUG

    ApplicationRawArguments rawArgs;
    if (!ParseArguments (arguments, &rawArgs) || !SemaArguments (rawArgs, &m_args)) {
        Log (LogSeverity::Error, L"Invalid arguments!");
        PrintUsage ();

        return false;
    }

    return true;
}

}   // namespace ETWP