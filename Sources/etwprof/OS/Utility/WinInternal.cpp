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

NTSTATUS NtQuerySystemInformation (SYSTEM_INFORMATION_CLASS SystemInformationClass,
                                   PVOID SystemInformation,
                                   ULONG SystemInformationLength,
                                   ULONG* ReturnLength)
{
    static bool queried;
    NtQuerySystemInformation_t pFunction = nullptr;
    if (!queried) {
        pFunction = GetNtQuerySystemInformationAddress ();

        // Note: error handling omitted (if for whatever reason this GetProcAddress fails, we just crash...)

        queried = true;
    }

    return pFunction (SystemInformationClass, SystemInformation, SystemInformationLength, ReturnLength);
}

NTSTATUS NtSetSystemInformation (SYSTEM_INFORMATION_CLASS SystemInformationClass,
                                 PVOID SystemInformation,
                                 ULONG SystemInformationLength)
{
    static bool queried;
    NtSetSystemInformation_t pFunction = nullptr;
    if (!queried) {
        pFunction = GetNtSetSystemInformationAddress ();

        // Note: error handling omitted (if for whatever reason this GetProcAddress fails, we just crash...)

        queried = true;
    }

    return pFunction (SystemInformationClass, SystemInformation, SystemInformationLength);
}

}	// namespace WinInternal
}   // namespace ETWP