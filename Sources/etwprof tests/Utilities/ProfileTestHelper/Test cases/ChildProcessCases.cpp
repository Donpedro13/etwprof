#include "OperationRegistrar.hpp"

#include <windows.h>

#include <memory>

namespace PTH {
namespace {

void StartPTHImpl (const std::wstring newOperation = {})
{
    // Massage the original command line of this process, so we can "swap" the operation parameter.
    // This is wrong on many levels, but it's just a test utility, and hey, it works!

    int numArgs = 0;
    LPWSTR* pArgs = CommandLineToArgvW (GetCommandLineW (), &numArgs);

    if (numArgs < 2)
        std::abort ();

    auto quotedAppend = [] (std::wstring& string, const std::wstring& toAppend) {
        string += L"\"" + toAppend + L"\" ";
    };

    std::wstring processPath = pArgs[0];
    std::wstring newCommandLine;
    std::wstring operation = newOperation.empty () ? pArgs[1] : newOperation;
    for (auto i = 0; i < numArgs; ++i) {
        const std::wstring arg = i == 1 ? operation : std::wstring (pArgs[i]);
        quotedAppend (newCommandLine, arg);   // quote everything "for good measure"
    }

    // Start the child in "async mode"; there is no way for anyone to signal its start event, as for that, the PID
    //  of the new process has to be known in advance.
    if (numArgs == 2)
        quotedAppend (newCommandLine, L"--nowait");

    LocalFree (pArgs);

    STARTUPINFOW startupInfo = {};
    startupInfo.cb = sizeof startupInfo;
    PROCESS_INFORMATION processInfo = {};

    std::unique_ptr<WCHAR[]> cmdLineCopy (new WCHAR[newCommandLine.length() + sizeof L'\0']);
    if (wcscpy_s (cmdLineCopy.get(), newCommandLine.length () + 1, newCommandLine.c_str ()) != 0)
        std::abort ();

    if (CreateProcessW (processPath.c_str (),
        cmdLineCopy.get (),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        nullptr,
        &startupInfo,
        &processInfo) == FALSE)
    {
        std::abort ();
    }

    CloseHandle (processInfo.hThread);

    CloseHandle (processInfo.hProcess);
}

void CreateChildren (uint16_t nChildren)
{
    for (auto i = 0; i < nChildren; ++i)
        StartPTHImpl (L"DoNothing");
}

static OperationRegistrator registrator1 (L"CreateChildProcess1", []() {
    CreateChildren (1);

    return true;
});

static OperationRegistrator registrator2 (L"CreateChildProcess128", []() {
    CreateChildren (128);

    return true;
});

static OperationRegistrator registrator3 (L"CreateChildProcessCascade5", []() {
    StartPTHImpl (L"CreateChildProcessCascade4");

    return true;
});

static OperationRegistrator registrator4 (L"CreateChildProcessCascade4", []() {
    StartPTHImpl(L"CreateChildProcessCascade3");

    return true;
});

static OperationRegistrator registrator5 (L"CreateChildProcessCascade3", []() {
    StartPTHImpl(L"CreateChildProcessCascade2");

    return true;
});

static OperationRegistrator registrator6 (L"CreateChildProcessCascade2", []() {
    StartPTHImpl (L"CreateChildProcess1");

    return true;
});

}	// namespace
}	// namespace PTH