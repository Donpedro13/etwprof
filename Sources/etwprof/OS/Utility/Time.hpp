#ifndef ETWP_TIME_HPP
#define ETWP_TIME_HPP

#include "OSTypes.hpp"

namespace ETWP {

using Timeout = uint32_t;
const Timeout Infinite = INFINITE;

TickCount operator""_tsec(unsigned long long seconds);
TickCount operator""_tmin(unsigned long long minutes);

TickCount GetTickCount ();

}   // namespace ETWP

#endif  // #ifndef ETWP_TIME_HPP