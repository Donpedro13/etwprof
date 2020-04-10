#include "WinInternal.hpp"

#include <type_traits>

namespace ETWP {
namespace WinInternal {

namespace {

using NtQuerySystemInformation_t =
std::add_pointer_t<NTSTATUS NTAPI (SYSTEM_INFORMATION_CLASS SystemInformationClass,
                                   PVOID SystemInformation,
                                   ULONG SystemInformationLength,
                                   ULONG* ReturnLength)>;

using NtSetSystemInformation_t =
std::add_pointer_t<NTSTATUS NTAPI (SYSTEM_INFORMATION_CLASS SystemInformationClass,
                                   PVOID SystemInformation,
                                   ULONG SystemInformationLength)>;

NtQuerySystemInformation_t GetNtQuerySystemInformationAddress ()
{
    static NtQuerySystemInformation_t pNtQuerySystemInformation = reinterpret_cast<NtQuerySystemInformation_t> (
                                                                    GetProcAddress (GetModuleHandleW (L"ntdll.dll"),
                                                                    "NtQuerySystemInformation"));

    return pNtQuerySystemInformation;
}

NtSetSystemInformation_t GetNtSetSystemInformationAddress ()
{
    static NtSetSystemInformation_t pNtSetSystemInformation = reinterpret_cast<NtSetSystemInformation_t> (
                                                                GetProcAddress (GetModuleHandleW (L"ntdll.dll"),
                                                                "NtSetSystemInformation"));

    return pNtSetSystemInformation;
}

}	// namespace

// Note: error handling omitted below (if for whatever reason GetProcAddress fails, we just crash...)
#pragma warning(push)
#pragma warning(disable: 6011)

NTSTATUS NtQuerySystemInformation (SYSTEM_INFORMATION_CLASS SystemInformationClass,
                                   PVOID SystemInformation,
                                   ULONG SystemInformationLength,
                                   ULONG* ReturnLength)
{
    static bool queried;
    static NtQuerySystemInformation_t pFunction = nullptr;
    if (!queried) {
        pFunction = GetNtQuerySystemInformationAddress ();

        queried = true;
    }

    return pFunction (SystemInformationClass, SystemInformation, SystemInformationLength, ReturnLength);
}

NTSTATUS NtSetSystemInformation (SYSTEM_INFORMATION_CLASS SystemInformationClass,
                                 PVOID SystemInformation,
                                 ULONG SystemInformationLength)
{
    static bool queried;
    static NtSetSystemInformation_t pFunction = nullptr;
    if (!queried) {
        pFunction = GetNtSetSystemInformationAddress ();

        queried = true;
    }

    return pFunction (SystemInformationClass, SystemInformation, SystemInformationLength);
}

#pragma warning(pop)

}	// namespace WinInternal
}   // namespace ETWP