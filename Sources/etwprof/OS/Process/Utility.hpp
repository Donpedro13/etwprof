#ifndef ETWP_PROCESS_UTILITY_HPP
#define ETWP_PROCESS_UTILITY_HPP

#include <windows.h>

#include <string>

namespace ETWP {

bool CreateProcessSynchronous (const std::wstring& processPath,
                               const std::wstring& args,
                               DWORD* pExitCodeOut = nullptr);

bool IsProcessElevated ();
bool AddProfilePrivilegeToProcessToken ();
bool IsProfilePrivilegePresentInToken (bool* pResultOut);

}   // namespace ETWP

#endif  // #ifndef #ifndef ETWP_PROCESS_UTILITY_HPP