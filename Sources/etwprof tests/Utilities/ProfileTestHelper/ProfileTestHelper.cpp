/*
  This small utility program takes an operation or test case name as an input parameter, and emits ETW events accordingly.
  
  These are the currently available operations: - RegETW	: Register the manifest-based test ETW provider
                                                - UnregETW	: Unregister the manifest-based test ETW provider
                                                - DoNothing : Perform no operation
  
  These are the currently available test cases: - BurnCPU5s		: Burn CPU for 5 seconds
                                                - TLEmitA		: Emit all events from Tracelogging provider "A"
                                                - TLEmitB		: Emit all events from Tracelogging provider "B"
                                                - TLEmitAB		: Emit all events both from Tracelogging provider "A" and "B"
                                                - MBEmitA		: Emit all events from manifest-based provider "A"
                                                - MBEmitB		: Emit all events from manifest-based provider "B"
                                                - MBEmitAB		: Emit all events both from manifest-based provider "A" and "B
                                                - EmitAll		: Emit all events from all providers
*/

#include <windows.h>
#include <tlhelp32.h>

#include <iostream>

#include "OperationRegistrar.hpp"

namespace {

BOOL WINAPI CtrlHandler (DWORD fdwCtrlType)
{
    static uint16_t count = 0;

    switch (fdwCtrlType) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
            if (++count >= 5)   // Allow for "manual" termination, if needed...
                return FALSE;

            std::wcout << L"Ignoring CTRL+C/CTRL+BREAK..." << std::endl;
            return TRUE;
    }

    return FALSE;
}

void Usage ()
{
    std::wcerr << L"Usage: ProfileTestHelper.exe <operation name> [--nowait]" << std::endl;
}

[[noreturn]] void Fail (const std::wstring& msg)
{
    std::wcerr << L"ERROR: " << msg << std::endl;

    exit (EXIT_FAILURE);
}

DWORD GetParentPID ()
{
    DWORD ownPID = GetCurrentProcessId ();

    HANDLE hSnapshot = CreateToolhelp32Snapshot (TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
        Fail (L"Unable to create process snapshot!");

    PROCESSENTRY32W processEntry;
    processEntry.dwSize = sizeof processEntry;

    if (!Process32FirstW (hSnapshot, &processEntry))
        Fail (L"Unable to get info for the first process in process snapshot!");

    do {
        if (processEntry.th32ProcessID == ownPID) {
            CloseHandle (hSnapshot);

            return processEntry.th32ParentProcessID;
        }
    } while (Process32NextW (hSnapshot, &processEntry));

    Fail (L"Unable to find process in process snapshot!");
}

void WaitForStartSignal ()
{
    // Construct the name of the event used for synchronization
    const DWORD parentPID = GetParentPID ();
    const DWORD pid = GetCurrentProcessId ();

    const std::wstring eventName = L"PTH_event_" + std::to_wstring (parentPID) + L"_" + std::to_wstring (pid);
    // Note: there is a race condition on who creates and who opens the event. This is because this process' PID is
    //   included in the name. From the parent's perspective: it first needs to start this child (it needs the child
    //   PID, after all), and tries to create the event, but then we might already get to this point here in the
    //   child process.
    HANDLE hEvent = CreateEventW (nullptr, FALSE, FALSE, eventName.c_str ());
    if (hEvent == nullptr)
        Fail (L"Unable to open event for synchronization!");

    const DWORD waitResult = WaitForSingleObject (hEvent, INFINITE);
    if (waitResult != WAIT_OBJECT_0)
        Fail (L"Wait failed on event!");

    CloseHandle (hEvent);
}

bool HandleArgCountCheck (int argc)
{
    if (argc != 2 && argc != 3) {
        std::wcout << L"Invalid number of arguments!" << std::endl;
        Usage();

        return false;
    } else {
        return true;
    }
}

bool HandleNoWaitArgCheck (wchar_t* pArg)
{
    if (_wcsicmp (pArg, L"--nowait") != 0) {
        std::wcout << L"Invalid second argument!" << std::endl;
        Usage();

        return false;
    }

    return true;
}

bool GetNoWaitArgCheckValue (wchar_t* pNoWaitArg)
{
    return _wcsicmp(pNoWaitArg, L"--nowait") == 0;
}

} // namespace

int wmain (int argc, wchar_t* argv[], wchar_t* /*envp[]*/)
{
    // Argument handling
    if (!HandleArgCountCheck (argc))
        return EXIT_FAILURE;

    bool waitForStartSignal = true;
    if (argc == 3) {
        if (!HandleNoWaitArgCheck (argv[2])) {
            return EXIT_FAILURE;
        }

        waitForStartSignal = !GetNoWaitArgCheckValue (argv[2]);
    }

    if (argc == 3) {
        if (_wcsicmp(argv[2], L"--nowait") != 0) {
            std::wcout << L"Invalid second argument!" << std::endl;
            Usage();

            return EXIT_FAILURE;
        }
    }

    // For ignoring interruptions which etwprof handles
    SetConsoleCtrlHandler (CtrlHandler, TRUE);

    // Wait until we are signaled to start our work (unless we are told otherwise by a command line parameter)
    if (waitForStartSignal)
        WaitForStartSignal ();

    const PTH::Operation* operation = PTH::OperationRegistrar::Instance ().Find (argv[1]);
    if (operation != nullptr) {
        if ((*operation) ())
            return EXIT_SUCCESS;
        else
            return EXIT_FAILURE;
    } else {
        std::wcout << L"Unknown operation: \"" << argv[1] << L"\"!" << std::endl << "Available operations:" << std::endl;

        PTH::OperationRegistrar::Instance ().Enumerate ([] (const std::wstring& name, const PTH::Operation*) {
                std::wcout << L"\t" << name << std::endl;

                return true;
            }
        );
    }
}