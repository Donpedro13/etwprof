#ifndef ETWP_ARGUMENTS_HPP
#define ETWP_ARGUMENTS_HPP

#include <windows.h>

#include <cstdint>
#include <string>
#include <vector>

namespace ETWP {

struct ApplicationRawArguments {
    bool verbose = false;
    bool debug = false;
    bool profile = false;
    bool noLogo = false;
    bool help = false;
    bool outputFile = false;
    bool outputDir = false;
    bool samplingRate = false;
    bool target = false;
    bool emulate = false;
    bool cswitch = false;
    bool minidump = false;
    bool minidumpFlags = false;
    bool userProviders = false;
    bool stackCache = false;

    std::wstring outputValue;
    std::wstring samplingRateValue;
    std::wstring targetValue;
    std::wstring inputETLPath;
    std::wstring compressionMode;
    std::wstring minidumpFlagsValue;
    std::wstring userProvidersValue;
};

struct ApplicationArguments {
    enum class CompressionMode {
        Invalid,
        Off,
        ETW,
        SevenZip
    };

    struct UserProviderInfo {
        enum IDType {
            GUID,
            RegisteredName, // Registered provider with a "known name"
            DynamicName     // A name prefixed with '*', we have to use this name to generate a GUID
        };

        std::wstring name;
        ::GUID       guid;
        IDType       type = RegisteredName;

        ULONGLONG keywordBitmask = 0;
        UCHAR     maxLevel = 0;
        bool      stack = false;
    };

    bool profile = false;
    bool noLogo = false;
    bool verbose = false;
    bool help = false;
    bool outputIsFile = false;
    bool targetIsPID = false;
    bool emulate = false;
    bool debug = false;
    bool cswitch = false;
    bool minidump = false;
    bool stackCache = false;

    DWORD                         targetPID;
    std::wstring                  targetName;
    std::wstring                  output;
    std::wstring                  inputETLPath;
    uint32_t                      samplingRate;
    CompressionMode               compressionMode = CompressionMode::Invalid;
    uint32_t                      minidumpFlags;
    std::vector<UserProviderInfo> userProviderInfos;
};

bool ParseArguments (const std::vector<std::wstring>& arguments,
                     ApplicationRawArguments* pArgumentsOut);
bool SemaArguments (const ApplicationRawArguments& parsedArgs,
                    ApplicationArguments* pArgumentsOut);

}   // namespace ETWP

#endif  // #ifndef ETWP_ARGUMENTS_HPP