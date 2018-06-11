#include "WinVersion.hpp"

#include <windows.h>
#include <VersionHelpers.h>

#include "Utility/Asserts.hpp"

namespace ETWP {

namespace {

struct WinVersion {
    DWORD majorVersion;
    DWORD minorVersion;
    DWORD buildNumber;
    bool  isDesktop;
};

bool GetWinVersionInternal (WinVersion* pVersionOut)
{
    ETWP_ASSERT (pVersionOut != nullptr);

    OSVERSIONINFOW versionInfo = {};
    versionInfo.dwOSVersionInfoSize = sizeof versionInfo;

#pragma warning (push)
#pragma warning (disable:4996 28159)
    if (ETWP_ERROR (!GetVersionExW (&versionInfo)))
        return false;
#pragma warning (pop)

    pVersionOut->majorVersion = versionInfo.dwMajorVersion;
    pVersionOut->minorVersion = versionInfo.dwMinorVersion;
    pVersionOut->buildNumber = versionInfo.dwBuildNumber;

    pVersionOut->isDesktop = !IsWindowsServer ();

    return true;
}

}   // namespace

BaseWinVersion GetWinVersion ()
{
    WinVersion version;
    if (!GetWinVersionInternal (&version))
        return BaseWinVersion::Unknown;

    // Win XP or earlier
    if (version.majorVersion < 6)
        return BaseWinVersion::Ancient;

    // Win Vista, 7, 8, or 8.1
    if (version.majorVersion == 6) {
        switch (version.minorVersion) {
            case 0:
                return BaseWinVersion::WinVista;
            case 1:
                return BaseWinVersion::Win7;
            case 2:
                return BaseWinVersion::Win8;
            case 3:
                return BaseWinVersion::Win81;
            case 4:
                return BaseWinVersion::Win10;   // Super early Win10 previews
            default:
                ETWP_DEBUG_BREAK_STR (L"Impossible Windows version!");
                return BaseWinVersion::Win81;
        }
    }

    // Windows 10
    if (version.majorVersion == 10)
        return BaseWinVersion::Win10;

    if (version.majorVersion > 10)
        return BaseWinVersion::FutureVersion;

    ETWP_DEBUG_BREAK_STR (L"Impossible Windows version!");
    return BaseWinVersion::Unknown;
}

WinVersionType GetWinVersionType ()
{
    WinVersion version;
    if (!GetWinVersionInternal (&version))
        return WinVersionType::Unknown;

    if (version.isDesktop)
        return WinVersionType::Desktop;
    else
        return WinVersionType::Server;
}

Win10Variant   GetWin10Variant ()
{
    WinVersion version;
    if (!GetWinVersionInternal (&version))
        return Win10Variant::Unknown;

    if (ETWP_ERROR (version.majorVersion != 10))
        return Win10Variant::Unknown;

    // See https://en.wikipedia.org/wiki/Windows_10_version_history for details
    if (version.buildNumber >= 16170)
        return Win10Variant::FutureUpdate;
    else if (version.buildNumber >= 14901)
        return Win10Variant::CreatorsUpdate;
    else if (version.buildNumber >= 11082)
        return Win10Variant::AnniversaryUpdate;
    else if (version.buildNumber >= 10525)
        return Win10Variant::NovemberUpdate;
    else if (version.buildNumber >= 10240)
        return Win10Variant::RTM;
    else
        return Win10Variant::PreRTM;
}

}   // namespace ETWP