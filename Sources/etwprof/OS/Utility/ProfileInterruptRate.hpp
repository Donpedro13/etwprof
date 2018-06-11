#ifndef ETWP_PROFILE_INTERRUPT_RATE_HPP
#define ETWP_PROFILE_INTERRUPT_RATE_HPP

#include <windows.h>

#include <chrono>

namespace ETWP {

// Floating point milliseconds
using ProfileRate = std::chrono::duration<double, std::milli>;
constexpr ProfileRate InvalidProfileRate (0);

bool GetGlobalSamplingRate (ProfileRate* pRateOut);
bool SetGlobalSamplingRate (const ProfileRate& newRate);

ProfileRate ConvertSamplingRateFromNative (ULONG rate);
ProfileRate ConvertSamplingRateFromHz (uint32_t freq);
ULONG ConvertSamplingRateToNative (const ProfileRate& rate);
double ConvertSamplingRateToHz (const ProfileRate& rate);

}   // namespace ETWP

#endif  // #ifndef ETWP_PROFILE_INTERRUPT_RATE_HPP