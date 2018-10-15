#include "Utility.hpp"

#include <windows.h>

#include <memory>

#include "Log/Logging.hpp"

#include "Utility/Asserts.hpp"
#include "Utility/OnExit.hpp"

namespace ETWP {

bool CreateProcessSynchronous (const std::wstring& processPath,
                               const std::wstring& args,
                               DWORD* pExitCodeOut /*= nullptr*/)
{
    const std::wstring realArgs = processPath + L" " + args;
    std::unique_ptr<WCHAR[]> cmdLineCopy (new WCHAR[realArgs.length () + sizeof '\0']);
    if (wcscpy_s (cmdLineCopy.get (),
                  realArgs.length () + sizeof '\0',
                  realArgs.c_str ()) != 0)
    {
        return false;
    }

    Log (LogSeverity::Debug, L"Trying to launch process " + processPath);

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
        return false;

    OnExit devNullHandleCloser ([&hDevNull]() { CloseHandle (hDevNull); });

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
        Log (LogSeverity::Warning, L"Unable to launch process " + processPath);

        return false;
    }

    CloseHandle (processInfo.hThread);

    OnExit processHandleCloser ([&processInfo] () { CloseHandle (processInfo.hProcess); });

    // Wait for the process to finish
    bool success = false;
    switch (WaitForSingleObject (processInfo.hProcess, INFINITE)) {
        case WAIT_OBJECT_0:
            success = true;
            break;
        case WAIT_FAILED:
            success = false;
            break;
        default:
            success = false;
            ETWP_DEBUG_BREAK_STR (L"Impossible value returned from WaitForSingleObject in CreateProcessSynchronous!");
    }

    if (success && pExitCodeOut != nullptr) {
        if (GetExitCodeProcess (processInfo.hProcess, pExitCodeOut) == FALSE)
            return false;
    }

    return success;
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

    OnExit handleGuard ([&hToken]() { CloseHandle (hToken); });

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

    OnExit handleGuard ([&hToken]() { CloseHandle (hToken); });

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