#ifndef ETWP_OS_TYPES_HPP
#define ETWP_OS_TYPES_HPP

#include <windows.h>

namespace ETWP {

using TickCount = ULONGLONG;

using PID = DWORD;
const PID InvalidPID = 0;

}   // namespace ETWP

#endif  // #ifndef ETWP_TIME_HPP