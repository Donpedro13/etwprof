/*
  This small utility program takes an operation or test case name as an input parameter, and emits ETW events accordingly.
  
  See the "Operations" and "Test cases" folders for the available operations and test cases.
*/

#include <windows.h>

#include <iostream>

#include "OperationRegistrar.hpp"
#include "Utility.hpp"

namespace PTH {
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
    std::wcerr << L"Usage: ProfileTestHelper.exe <operation name> [--nowait | --wait]" << std::endl;
}

WinHandle GetStartSignalHandle ()
{
    // Construct the name of the event used for synchronization
    const DWORD parentPID = GetParentPID ();
    const DWORD pid = GetCurrentProcessId ();

    const std::wstring eventName = L"PTH_event_" + std::to_wstring (parentPID) + L"_" + std::to_wstring (pid);
    // Note: there is a race condition on who creates and who opens the event. This is because this process' PID is
    //   included in the name. From the parent's perspective: it first needs to start this child (it needs the child
    //   PID, after all), and tries to create the event, but then we might already get to this point here in the
    //   child process.
    HANDLE hEvent = CreateEventW (nullptr, TRUE, FALSE, eventName.c_str ());
    if (hEvent == nullptr)
        Fail (L"Unable to open event for synchronization!");

    return hEvent;
}

WinHandle GetCancelSignalHandle ()
{
    const std::wstring eventName = L"PTH_event_global_cancel";
    HANDLE hEvent = CreateEventW (nullptr, TRUE, FALSE, eventName.c_str ());
    if (hEvent == nullptr)
        Fail (L"Unable to open event for synchronization!");

    return hEvent;
}

enum class SignalResult {
    Start,
    Cancel
};

SignalResult WaitForStartOrCancelSignal ()
{
    WinHandle hStart = GetStartSignalHandle ();
    WinHandle hCancel = GetCancelSignalHandle ();
    HANDLE handles[] = { hStart.Get (), hCancel.Get () };

    const DWORD waitResult = WaitForMultipleObjects (2, handles, FALSE, INFINITE);
    switch (waitResult) {
        case WAIT_OBJECT_0:
            return SignalResult::Start;

        case WAIT_OBJECT_0 + 1:
            return SignalResult::Cancel;

        default:
            std::abort ();
    }
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
    if (_wcsicmp (pArg, L"--nowait") != 0 && _wcsicmp(pArg, L"--wait") != 0) {
        std::wcout << L"Invalid second argument!" << std::endl;
        Usage ();

        return false;
    }

    return true;
}

bool GetNoWaitArgValue (wchar_t* pNoWaitArg)
{
    return _wcsicmp (pNoWaitArg, L"--nowait") == 0;
}

} // namespace
} // namespace PTH

int wmain (int argc, wchar_t* argv[], wchar_t* /*envp[]*/)
{
    // Argument handling
    if (!PTH::HandleArgCountCheck (argc))
        return EXIT_FAILURE;

    bool waitForStartOrCancelSignal = true;
    if (argc == 3) {
        if (!PTH::HandleNoWaitArgCheck (argv[2])) {

            return EXIT_FAILURE;
        }

        waitForStartOrCancelSignal = !PTH::GetNoWaitArgValue (argv[2]);
    }

    // For ignoring interruptions which etwprof handles
    SetConsoleCtrlHandler (PTH::CtrlHandler, TRUE);

    // Wait until we are signaled to start our work (unless we are told otherwise by a command line parameter)
    if (waitForStartOrCancelSignal)
        if (PTH::WaitForStartOrCancelSignal () == PTH::SignalResult::Cancel)
            return EXIT_SUCCESS;

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

        return EXIT_FAILURE;
    }
}