#ifndef ETWP_TIME_HPP
#define ETWP_TIME_HPP

#include <windows.h>

namespace ETWP {

using TickCount = ULONGLONG;

TickCount operator""_tsec(unsigned long long seconds);
TickCount operator""_tmin(unsigned long long minutes);

TickCount GetTickCount ();

}   // namespace ETWP

#endif  // #ifndef ETWP_TIME_HPP