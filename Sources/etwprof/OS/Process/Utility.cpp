#include "Utility.hpp"

#include <windows.h>

#include <memory>

#include "Utility/Asserts.hpp"
#include "Utility/OnExit.hpp"

namespace ETWP {

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