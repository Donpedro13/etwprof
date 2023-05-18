#include "Time.hpp"

namespace ETWP {

TickCount operator""_tsec (unsigned long long seconds)
{
	return seconds * 1'000;
}

TickCount operator""_tmin (unsigned long long minutes)
{
	return 60_tsec * minutes;
}

TickCount GetTickCount()
{
	return GetTickCount64 ();
}

}   // namespace ETWP