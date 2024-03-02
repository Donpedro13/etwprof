#include "OperationRegistrar.hpp"

#include <windows.h>

#include <memory>

#include "Utility.hpp"

namespace PTH {
namespace {

void QuotedAppend (std::wstring& string, const std::wstring& toAppend)
{
    string += L"\"" + toAppend + L"\" ";
}

WinHandle StartProcessImpl (const std::wstring& processPath, const std::wstring& commandLine, bool createNoWindow = false)
{
    STARTUPINFOW startupInfo = {};
    startupInfo.cb = sizeof startupInfo;
    PROCESS_INFORMATION processInfo = {};

    std::wstring actualCommandLine;
    QuotedAppend (actualCommandLine, processPath);
    actualCommandLine += L' ' + commandLine;

    std::unique_ptr<WCHAR[]> cmdLineCopy (new WCHAR[actualCommandLine.length () + sizeof L'\0']);
    if (wcscpy_s (cmdLineCopy.get (), actualCommandLine.length () + 1, actualCommandLine.c_str ()) != 0)
        std::abort ();

    if (CreateProcessW (processPath.c_str (),
                        cmdLineCopy.get (),
                        nullptr,
                        nullptr,
                        FALSE,
                        createNoWindow ? CREATE_NO_WINDOW : 0,
                        nullptr,
                        nullptr,
                        &startupInfo,
                        &processInfo) == FALSE)
    {
        std::abort ();
    }

    CloseHandle (processInfo.hThread);

    return processInfo.hProcess;
}

WinHandle StartPTHImpl (const std::wstring newOperation = {}, bool wait = false)
{
    // Massage the original command line of this process, so we can "swap" the operation parameter.
    // This is wrong on many levels, but it's just a test utility, and hey, it works!

    int numArgs = 0;
    LPWSTR* pArgs = CommandLineToArgvW (GetCommandLineW (), &numArgs);

    if (numArgs < 2 || numArgs > 3)
        std::abort ();

    std::wstring commandLine;
    const std::wstring operation = newOperation.empty () ? pArgs[1] : newOperation;
    const std::wstring waitParam = wait ? L"--wait" : L"--nowait";
    QuotedAppend (commandLine, operation);
    QuotedAppend(commandLine, waitParam);

    const std::wstring processPath = pArgs[0];
    LocalFree (pArgs);

    return StartProcessImpl (processPath, commandLine);
}

std::vector<WinHandle> StartPTHChildren (uint16_t nChildren, bool wait = false)
{
    std::vector<WinHandle> result;

    for (auto i = 0; i < nChildren; ++i)
        result.push_back (StartPTHImpl (L"DoNothing", wait));

    return result;
}

void WaitForProcess (WinHandle& hProcess)
{
    DWORD waitResult = WaitForSingleObject (hProcess.Get (), INFINITE);
        if (waitResult != WAIT_OBJECT_0)
            std::abort ();
}

void StartAndWaitForPTHChildren (uint16_t nChildren)
{
    auto hChildren = StartPTHChildren (nChildren, true);
    for (auto i = 0; i < nChildren; ++i)
        WaitForProcess (hChildren[i]);
}

WinHandle RunCommandWithCmdAsChild (const std::wstring& command)
{
    return StartProcessImpl (L"C:\\Windows\\System32\\cmd.exe", L"/C" + std::wstring (L" ") + command, true);
}

static OperationRegistrator registrator1 (L"CreateChildProcess1WaitFor", []() {
    WaitForProcess (StartPTHChildren (1)[0]);

    return true;
});

static OperationRegistrator registrator2 (L"CreateChildProcess1", []() {
    StartPTHChildren (1);

    return true;
});

static OperationRegistrator registrator3 (L"CreateChildProcessMixed5", []() {
    StartPTHChildren (4);
    RunCommandWithCmdAsChild (L"");

    return true;
});

static OperationRegistrator registrator4 (L"CreateChildProcess128", []() {
    StartPTHChildren (128);

    return true;
});

static OperationRegistrator registrator5 (L"CreateChildProcessCascade5", []() {
    StartPTHImpl (L"CreateChildProcessCascade4");

    return true;
});

static OperationRegistrator registrator6 (L"CreateChildProcessCascade4", []() {
    StartPTHImpl(L"CreateChildProcessCascade3");

    return true;
});

static OperationRegistrator registrator7 (L"CreateChildProcessCascade3", []() {
    StartPTHImpl(L"CreateChildProcessCascade2");

    return true;
});

static OperationRegistrator registrator8 (L"CreateChildProcessCascade2", []() {
    StartPTHImpl (L"CreateChildProcess1");

    return true;
});

// The point of "process tree operations" is to create certain process trees, *before* profiling starts. These
//  processes wait, so etwprof has a chance to start profiling while the process tree is "present" in its original
//  state.
// We achieve this by creating several waiting processes (that's the operation itself, if you will). Created PTH
//  processes (also) wait on the "global cancel event", that's when they know when they need to exit. In other words:
//  child PTHs' operations will get cancelled.

static OperationRegistrator registrator9 (L"CreateProcessTreeWith5PTHs", []() {
    StartAndWaitForPTHChildren (5);

    return true;
});

static OperationRegistrator registrator10 (L"CreateProcess5TreeMixed", []() {
    // We create a cmd.exe in our tree, and make it wait by starting a PTH process and making it wait for its completion
    auto hCmd = RunCommandWithCmdAsChild (L"start /w " + GetExePath () + L" DoNothing");
    StartAndWaitForPTHChildren (4);
    WaitForProcess (hCmd);

    return true;
});

}	// namespace
}	// namespace PTH