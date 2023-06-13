#include "Utility.hpp"

#include <windows.h>

#include <memory>

#include "Log/Logging.hpp"

#include "Utility/Asserts.hpp"
#include "Utility/OnExit.hpp"
#include "Utility/Result.hpp"

namespace ETWP {

namespace {

// /analyze is unaware that we close the process handle in an OnExit object...
#pragma warning (push)
#pragma warning (disable: 6335)
Result<ProcessRef> CreateProcessImplNoOutput (const std::wstring& processPath,
                                              const std::wstring& args,
                                              ProcessRef::Options options)
{
    const std::wstring realArgs = processPath + L" " + args;
    std::unique_ptr<WCHAR[]> cmdLineCopy (new WCHAR[realArgs.length () + sizeof L'\0']);
    if (wcscpy_s (cmdLineCopy.get (),
                  realArgs.length () + 1,
                  realArgs.c_str ()) != 0)
    {
        return Error (L"Unable to construct command line!");
    }

    STARTUPINFOW startupInfo = {};
    startupInfo.cb = sizeof startupInfo;
    PROCESS_INFORMATION processInfo = {};

    SECURITY_ATTRIBUTES secAttr = {};
    secAttr.nLength = sizeof secAttr;
    secAttr.lpSecurityDescriptor = nullptr;
    secAttr.bInheritHandle = TRUE;

    HANDLE hDevNull = CreateFileW (L"NUL",
                                   GENERIC_WRITE,
                                   0,
                                   &secAttr,
                                   OPEN_EXISTING,
                                   0,
                                   nullptr);

    if (hDevNull == INVALID_HANDLE_VALUE)
        return Error (L"Unable to get handle for NUL!");;

    OnExit devNullHandleCloser ([&hDevNull] () { CloseHandle (hDevNull); });

    startupInfo.hStdOutput = hDevNull;
    startupInfo.hStdError = hDevNull;
    startupInfo.hStdInput = nullptr;
    startupInfo.dwFlags |= STARTF_USESTDHANDLES;

    if (CreateProcessW (processPath.c_str (),
                        cmdLineCopy.get (),
                        nullptr,
                        nullptr,
                        TRUE,
                        0,
                        nullptr,
                        nullptr,
                        &startupInfo,
                        &processInfo) == FALSE)
    {
        return Error (L"CreateProcessW failed!");
    }

    CloseHandle (processInfo.hThread);

    OnExit processHandleCloser ([&processInfo] () { CloseHandle (processInfo.hProcess); });

    return ProcessRef (processInfo.dwProcessId, options);
}
#pragma warning (pop)

}   // namespace

bool CreateProcessSynchronousNoOutput (const std::wstring& processPath,
                                       const std::wstring& args,
                                       DWORD* pExitCodeOut /*= nullptr*/)
{
    Result<ProcessRef> createResult = CreateProcessImplNoOutput (processPath,
                                                                 args,
                                                                 ProcessRef::Synchronize);
    if (!createResult.has_value ()) {
        Log (LogSeverity::Warning, L"Unable to launch process " + processPath + L": " + createResult.error ());

        return false;
    }

    // Wait for the process to finish
    createResult->Wait ();

    *pExitCodeOut = createResult->GetExitCode ();

    return true;
}

Result<ProcessRef> CreateProcessAsynchronousNoOutput (const std::wstring& processPath,
                                                      const std::wstring& args,
                                                      ProcessRef::Options options)
{
    return CreateProcessImplNoOutput (processPath, args, options);
}

bool IsProcessElevated ()
{
    bool elevated = false;
    HANDLE hToken = nullptr;
    if (ETWP_VERIFY (OpenProcessToken (GetCurrentProcess (), TOKEN_QUERY, &hToken) != FALSE)) {
        TOKEN_ELEVATION elevation;
        DWORD cbSize = sizeof (TOKEN_ELEVATION);
        if (ETWP_VERIFY (GetTokenInformation (hToken, TokenElevation, &elevation, sizeof elevation, &cbSize) != FALSE))
            elevated = elevation.TokenIsElevated != 0;
    }

    if (hToken != nullptr)
        CloseHandle (hToken);

    return elevated;
}

bool AddProfilePrivilegeToProcessToken ()
{
    HANDLE hToken;
    if (ETWP_ERROR (OpenProcessToken (GetCurrentProcess (), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken) == FALSE))
        return false;

    OnExit handleGuard ([&hToken] () { CloseHandle (hToken); });

    std::unique_ptr<char[]> pMem (new char[sizeof (TOKEN_PRIVILEGES) + sizeof (LUID_AND_ATTRIBUTES)]);
    PTOKEN_PRIVILEGES privileges = reinterpret_cast<PTOKEN_PRIVILEGES> (pMem.get ());
    privileges->PrivilegeCount = 1;
    privileges->Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;

    if (ETWP_ERROR (LookupPrivilegeValueW (nullptr, SE_SYSTEM_PROFILE_NAME, &(privileges->Privileges[0]).Luid) == FALSE))
        return false;

    if (ETWP_ERROR (AdjustTokenPrivileges (hToken, FALSE, privileges, 0, nullptr, 0) == FALSE))
        return false;

    return true;
}

bool IsProfilePrivilegePresentInToken (bool* pResultOut)
{
    ETWP_ASSERT (pResultOut != nullptr);

    HANDLE hToken;
    if (ETWP_ERROR (OpenProcessToken (GetCurrentProcess (), TOKEN_QUERY, &hToken) == FALSE))
        return false;

    OnExit handleGuard ([&hToken] () { CloseHandle (hToken); });

    PRIVILEGE_SET privileges;
    privileges.PrivilegeCount = 1;
    privileges.Control = 0;

    if (ETWP_ERROR (LookupPrivilegeValueW (nullptr, SE_SYSTEM_PROFILE_NAME, &(privileges.Privilege[0]).Luid) == FALSE))
        return false;

    BOOL result;
    if (ETWP_ERROR (PrivilegeCheck (hToken, &privileges, &result) == FALSE))
        return false;

    *pResultOut = result == FALSE ? false : true;

    return true;
}

}   // namespace ETWP