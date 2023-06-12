#ifndef ETWP_PROCESS_UTILITY_HPP
#define ETWP_PROCESS_UTILITY_HPP

#include <windows.h>

#include <expected>
#include <string>

#include "OS/Process/ProcessRef.hpp"

namespace ETWP {

bool CreateProcessSynchronousNoOutput (const std::wstring& processPath,
                                       const std::wstring& args,
                                       DWORD* pExitCodeOut = nullptr);
std::expected<ProcessRef, std::wstring> CreateProcessAsynchronousNoOutput (const std::wstring& processPath,
                                                                           const std::wstring& args,
                                                                           ProcessRef::Options options);

bool IsProcessElevated ();
bool AddProfilePrivilegeToProcessToken ();
bool IsProfilePrivilegePresentInToken (bool* pResultOut);

}   // namespace ETWP

#endif  // #ifndef #ifndef ETWP_PROCESS_UTILITY_HPP