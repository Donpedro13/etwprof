#ifndef ETWP_WIN_VERSION_HPP
#define ETWP_WIN_VERSION_HPP


namespace ETWP {

// You can use numeric comparisons on these values
enum class BaseWinVersion {
    Ancient,    // :)
    WinVista,
    Win7,
    Win8,
    Win81,
    Win10,
    FutureVersion,
    Unknown
};

// You can use numeric comparisons on these values
enum class Win10Variant {
    PreRTM,
    RTM,
    NovemberUpdate,
    AnniversaryUpdate,
    CreatorsUpdate,
    FutureUpdate,
    Unknown
};

enum class WinVersionType {
    Unknown,
    Desktop,
    Server,
    Other
};

BaseWinVersion GetWinVersion ();
WinVersionType GetWinVersionType ();
Win10Variant   GetWin10Variant ();

}   // namespace ETWP

#endif  // #ifndef #define ETWP_WIN_VERSION_HPP
