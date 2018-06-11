#ifndef ETWP_MINIDUMP_HPP
#define ETWP_MINIDUMP_HPP

#include <windows.h>

#include <cstdint>
#include <string>

namespace ETWP {

bool CreateMinidump (const std::wstring& dumpPath,
                     HANDLE hProcess,
                     DWORD processPID,
                     uint32_t dumpFlags,
                     std::wstring* pErrorOut);

}   // namespace ETWP

#endif  // #ifndef ETWP_MINIDUMP_HPP