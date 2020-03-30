#include "ProfileInterruptRate.hpp"

#include <type_traits>

#include "Log/Logging.hpp"
#include "OS/Utility/WinInternal.hpp"
#include "Utility/Macros.hpp"

namespace ETWP {

bool GetGlobalSamplingRate (ProfileRate* pRateOut)
{
    WinInternal::EVENT_TRACE_TIME_PROFILE_INFORMATION profileTimeInfo;
    profileTimeInfo.EventTraceInformationClass = WinInternal::EventTraceTimeProfileInformation;
    profileTimeInfo.ProfileInterval = 0;

    if (WinInternal::NtQuerySystemInformation (WinInternal::SystemPerformanceTraceInformation,
                                               &profileTimeInfo,
                                               sizeof profileTimeInfo,
                                               nullptr)
        != 0)   // STATUS_SUCCESS
    {
        Log (LogSeverity::Warning,
            L"Cannot query current profile rate (NtQuerySystemInformation "
            L"returned error)");

        return false;
    }

    *pRateOut = std::chrono::duration_cast<ProfileRate> (std::chrono::nanoseconds (profileTimeInfo.ProfileInterval *
                                                                                    100));

    return true;
}

bool SetGlobalSamplingRate (const ProfileRate& newRate)
{
    WinInternal::EVENT_TRACE_TIME_PROFILE_INFORMATION profileTimeInfo;
    profileTimeInfo.EventTraceInformationClass = WinInternal::EventTraceTimeProfileInformation;
    profileTimeInfo.ProfileInterval = 
        static_cast<ULONG> (std::chrono::duration_cast<std::chrono::nanoseconds> (newRate).count () / 100);

    if (WinInternal::NtSetSystemInformation (WinInternal::SystemPerformanceTraceInformation,
                                             &profileTimeInfo,
                                             sizeof profileTimeInfo)
        != 0)   // STATUS_SUCCESS
    {
        Log (LogSeverity::Warning,
            L"Cannot set current profile rate (NtSetSystemInformation"
            L" returned error)");

        return false;
    }

    return true;
}

ProfileRate ConvertSamplingRateFromNative (ULONG rate)
{
    return std::chrono::duration_cast<ProfileRate>
        (std::chrono::nanoseconds (rate * 100));
}

ProfileRate ConvertSamplingRateFromHz (uint32_t freq)
{
    return freq == 0 ?
        InvalidProfileRate :
        ProfileRate (static_cast<double> (1) / freq * 1'000);   // Hertz to milliseconds
}

ULONG ConvertSamplingRateToNative (const ProfileRate& rate)
{
    return rate == InvalidProfileRate ?
        0 :
        static_cast<ULONG> (rate.count () * 1'000'000 / 100);    // Convert to 100 ns units
}

double ConvertSamplingRateToHz (const ProfileRate& rate)
{
    return rate == InvalidProfileRate ?
        0 :
        1 / (rate.count () / 1'000); // Convert to seconds, then return 1 / result, aka. Hz
}

}   // namespace ETWP