#include "ProfileInterruptRate.hpp"

#include <type_traits>

#include "Log/Logging.hpp"
#include "Utility/Macros.hpp"

namespace ETWP {

// Undocumented Windows structures etc. used throughout this file
namespace WinInternal {

// Thanks to Geoff Chappell (www.geoffchappell.com) for these structs and enums
enum SYSTEM_INFORMATION_CLASS {
    // ...
    SystemPerformanceTraceInformation = 0x1F    // 31
    // ...
};

enum EVENT_TRACE_INFORMATION_CLASS {
    // ...
    EventTraceTimeProfileInformation = 3
    // ...
};

struct EVENT_TRACE_TIME_PROFILE_INFORMATION {
    EVENT_TRACE_INFORMATION_CLASS EventTraceInformationClass;
    ULONG ProfileInterval;
};

using NtSetSystemInformation_t =
    std::add_pointer_t<NTSTATUS NTAPI (
        SYSTEM_INFORMATION_CLASS SystemInformationClass,
        PVOID SystemInformation,
        ULONG SystemInformationLength)>;

using NtQuerySystemInformation_t =
    std::add_pointer_t<NTSTATUS NTAPI (
        SYSTEM_INFORMATION_CLASS SystemInformationClass,
        PVOID SystemInformation,
        ULONG SystemInformationLength,
        ULONG *ReturnLength)>;

NtQuerySystemInformation_t GetNtQuerySystemInformationAddress ()
{
    static NtQuerySystemInformation_t pNtQuerySystemInformation =
        reinterpret_cast<NtQuerySystemInformation_t> (
            GetProcAddress (GetModuleHandleW (L"ntdll.dll"),
                "NtQuerySystemInformation"));

    return pNtQuerySystemInformation;
}

NtSetSystemInformation_t GetNtSetSystemInformationAddress ()
{
    static NtSetSystemInformation_t pNtSetSystemInformation =
        reinterpret_cast<NtSetSystemInformation_t> (
            GetProcAddress (GetModuleHandleW (L"ntdll.dll"),
                "NtSetSystemInformation"));

    return pNtSetSystemInformation;
}

}   // namespace WinInternal

bool GetGlobalSamplingRate (ProfileRate* pRateOut)
{
    auto pNtQuerySystemInformation =
        WinInternal::GetNtQuerySystemInformationAddress ();

    if (pNtQuerySystemInformation == nullptr) {
        Log (LogSeverity::Warning,
            L"Cannot query address of NtQuerySystemInformation in " ETWP_WFUNC);

        return false;
    }

    WinInternal::EVENT_TRACE_TIME_PROFILE_INFORMATION profileTimeInfo;
    profileTimeInfo.EventTraceInformationClass =
        WinInternal::EventTraceTimeProfileInformation;
    profileTimeInfo.ProfileInterval = 0;

    if (pNtQuerySystemInformation (WinInternal::SystemPerformanceTraceInformation,
        &profileTimeInfo,
        sizeof profileTimeInfo,
        nullptr) != 0)   // STATUS_SUCCESS
    {
        Log (LogSeverity::Warning,
            L"Cannot query current profile rate (NtQuerySystemInformation "
            L"returned error)");

        return false;
    }

    *pRateOut =
        std::chrono::duration_cast<ProfileRate> (std::chrono::nanoseconds (
            profileTimeInfo.ProfileInterval * 100));

    return true;
}

bool SetGlobalSamplingRate (const ProfileRate& newRate)
{
    auto pNtSetSystemInformation =
        WinInternal::GetNtSetSystemInformationAddress ();

    if (pNtSetSystemInformation == nullptr) {
        Log (LogSeverity::Warning,
            L"Cannot query address of NtSetSystemInformation in " ETWP_WFUNC);

        return false;
    }

    WinInternal::EVENT_TRACE_TIME_PROFILE_INFORMATION profileTimeInfo;
    profileTimeInfo.EventTraceInformationClass =
        WinInternal::EventTraceTimeProfileInformation;
    profileTimeInfo.ProfileInterval = static_cast<ULONG> (
        std::chrono::duration_cast<std::chrono::nanoseconds>
            (newRate).count () / 100);

    if (pNtSetSystemInformation (WinInternal::SystemPerformanceTraceInformation,
        &profileTimeInfo,
        sizeof profileTimeInfo) != 0)   // STATUS_SUCCESS
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