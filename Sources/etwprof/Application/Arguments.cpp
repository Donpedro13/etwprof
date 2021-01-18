#include "Arguments.hpp"

#include "Application.hpp"

#include "Log/Logging.hpp"

#include "OS/ETW/Utils.hpp"

#include "OS/FileSystem/Utility.hpp"

#include "OS/Version/WinVersion.hpp"

#include "Utility/Asserts.hpp"
#include "Utility/GUID.hpp"
#include "Utility/ResponseFile.hpp"
#include "Utility/StringUtils.hpp"

// Note: Arguments are processed by our own hand-rolled logic. This has several
//   advantages and disadvantages.
// Pros: - No 3rd party dependencies introduced
//       - Argument checking is really strict
//       - Error messages are *super nice*
// Cons: - The code is hard to comprehend
//       - Maintenance is *really* cumbersome (adding/removing options, etc.)
//
// In the future, switching to a 3rd party solution might be considered.

namespace ETWP {

namespace {

void LogFailedParse (const std::wstring& message, const std::wstring& arg)
{
    Log (LogSeverity::Warning, L"Argument parsing failed (" + arg + L"): " + message);
}

void LogFailedParse (const std::wstring& message)
{
    Log (LogSeverity::Warning, L"Argument parsing failed: " + message);
}

/*void LogFailedSema (const std::wstring& message,
                           const std::wstring& arg)
{
    Log (LogSeverity::Info, L"Argument error (" + arg + L"): " + message);
}*/

void LogFailedSema (const std::wstring& message)
{
    Log (LogSeverity::Warning, L"Argument error: " + message);
}

bool IsRespFileDirective (const std::wstring& str)
{
    return str.compare (0, 1, L"@") == 0;
}

bool IsArg (const std::wstring& str)
{    
    ETWP_ASSERT (!str.empty ()); // Impossible

    return str.compare (0, 1, L"-") == 0;
}

bool IsCommand (const std::wstring& str)
{
    return !IsArg (str);
}

bool IsShortArg (const std::wstring& arg)
{
    ETWP_ASSERT (IsArg (arg));

    if (ETWP_ERROR (arg.length () < 2))
        return false;
    
    if (arg.compare (0, 2, L"--") == 0)
        return false;
    else if (arg.compare (0, 1, L"-") == 0)
        return true;

    ETWP_DEBUG_BREAK_STR (L"Unexpected argument in " __FUNCTIONW__);

    return false;
}

bool IsAssignmentArg (const std::wstring& arg)
{
    ETWP_ASSERT (IsArg (arg));

    return arg.find (L"=") != arg.npos;
}

std::wstring GetRespFilePathFromDirective (const std::wstring& directive)
{
    ETWP_ASSERT (IsRespFileDirective (directive));

    return directive.substr (1);
}

std::wstring GetArgName (const std::wstring& arg)
{
    ETWP_ASSERT (IsArg (arg));

    std::wstring argWoPrefix;
    if (IsShortArg (arg)) {
        argWoPrefix = arg.substr (1);
    } else {
        argWoPrefix = arg.substr (2);
    }

    // Check if there is an assignment in the arg
    size_t equalsSignPos = argWoPrefix.find (L"=");
    return equalsSignPos == argWoPrefix.npos ? argWoPrefix : argWoPrefix.substr (0, equalsSignPos);
}

std::wstring GetArgValue (const std::wstring& arg)
{
    ETWP_ASSERT (IsAssignmentArg (arg));

    return arg.substr (arg.find (L"=") + 1);
}

bool ParseShortAssignmentArg (const std::wstring& arg, ApplicationRawArguments* pArgumentsOut)
{
    ETWP_ASSERT (IsShortArg (arg));
    ETWP_ASSERT (IsAssignmentArg (arg));

    if (GetArgValue (arg).empty ()) {
        LogFailedParse (L"Assignment argument value cannot be empty!", arg);

        return false;
    }

    if (GetArgName (arg) == L"o") {
        pArgumentsOut->outputFile = true;
        pArgumentsOut->outputValue = GetArgValue (arg);

        return true;
    } else if (GetArgName (arg) == L"t") {
        pArgumentsOut->target = true;
        pArgumentsOut->targetValue = GetArgValue (arg);

        return true;
    }

    LogFailedParse (L"Unknown short form value argument", arg);

    return false;
}

bool ParseShortNonAssignmentArg (const std::wstring& arg, ApplicationRawArguments* pArgumentsOut)
{
    ETWP_ASSERT (IsShortArg (arg));
    ETWP_ASSERT (!IsAssignmentArg (arg));

    std::wstring argName = GetArgName (arg);
    // -h ; -v ; -d ; -m
    if (argName == L"h") {
        pArgumentsOut->help = true;

        return true;
    } else if (argName == L"v") {
        pArgumentsOut->verbose = true;

        return true;
    } else if (argName == L"d") {
        pArgumentsOut->debug = true;

        return true;
    } else if (argName == L"m") {
        pArgumentsOut->minidump = true;

        return true;
    }

    LogFailedParse (L"Unknown short form non-value argument!", arg);

    return false;
}

bool ParseShortArg (const std::wstring& arg, ApplicationRawArguments* pArgumentsOut)
{
    if (IsAssignmentArg (arg))
        return ParseShortAssignmentArg (arg, pArgumentsOut);
    else
        return ParseShortNonAssignmentArg (arg, pArgumentsOut);
}

bool ParseLongAssignmentArg (const std::wstring& arg, ApplicationRawArguments* pArgumentsOut)
{
    ETWP_ASSERT (!IsShortArg (arg));
    ETWP_ASSERT (IsAssignmentArg (arg));

    if (GetArgValue (arg).empty ()) {
        LogFailedParse (L"Assignment argument value cannot be empty!", arg);

        return false;
    }

    std::wstring argName = GetArgName (arg);
    if (argName == L"output") {
        pArgumentsOut->outputFile = true;
        pArgumentsOut->outputValue = GetArgValue (arg);

        return true;
    } else if (argName == L"outdir") {
        pArgumentsOut->outputDir = true;
        pArgumentsOut->outputValue = GetArgValue (arg);

        return true;
    } else if (argName == L"target") {
        pArgumentsOut->target = true;
        pArgumentsOut->targetValue = GetArgValue (arg);

        return true;
    } else if (argName == L"rate") {
        pArgumentsOut->samplingRate = true;
        pArgumentsOut->samplingRateValue = GetArgValue (arg);

        return true;
    } else if (argName == L"emulate") {
        pArgumentsOut->emulate = true;
        pArgumentsOut->inputETLPath = GetArgValue (arg);

        return true;
    } else if (argName == L"compress") {
        pArgumentsOut->compressionMode = GetArgValue (arg);

        return true;
    } else if (argName == L"mflags") {
        pArgumentsOut->minidumpFlags = true;
        pArgumentsOut->minidumpFlagsValue = GetArgValue (arg);

        return true;
    } else if (argName == L"enable") {
        pArgumentsOut->userProviders = true;
        pArgumentsOut->userProvidersValue = GetArgValue (arg);

        return true;
    }

    LogFailedParse (L"Unknown value argument!", arg);

    return false;
}

bool ParseLongNonAssignmentArg (const std::wstring& arg, ApplicationRawArguments* pArgumentsOut)
{
    ETWP_ASSERT (!IsShortArg (arg));
    ETWP_ASSERT (!IsAssignmentArg (arg));

    std::wstring argName = GetArgName (arg);
    // --help ; --verbose ; --nologo ; --debug ; --cswitch ; --mdump ; --scache ; --noaction
    if (argName == L"help") {
        pArgumentsOut->help = true;

        return true;
    } else if (argName == L"verbose") {
        pArgumentsOut->verbose = true;

        return true;
    } else if (argName == L"nologo") {
        pArgumentsOut->noLogo = true;

        return true;
    } else if (argName == L"debug") {
        pArgumentsOut->debug = true;

        return true;
    } else if (argName == L"cswitch") {
        pArgumentsOut->cswitch = true;

        return true;
    } else if (argName == L"mdump") {
        pArgumentsOut->minidump = true;

        return true;
	} else if (argName == L"scache") {
		pArgumentsOut->stackCache = true;

		return true;
	} else if (argName == L"noaction") {
		pArgumentsOut->noAction = true;

		return true;
	}

    LogFailedParse (L"Unknown non-value argument!", arg);

    return false;
}

bool ParseLongArg (const std::wstring& arg, ApplicationRawArguments* pArgumentsOut)
{
    if (IsAssignmentArg (arg))
        return ParseLongAssignmentArg (arg, pArgumentsOut);
    else
        return ParseLongNonAssignmentArg (arg, pArgumentsOut);
}

bool SemaOutputPath (const ApplicationRawArguments& parsedArgs, ApplicationArguments* pArgumentsOut)
{
    // Check if both an output file and an output dir is specified
    if (parsedArgs.outputFile && parsedArgs.outputDir) {
        LogFailedSema (L"Both an output file and a directory is specified!");

        return false;
    }

    if (!parsedArgs.outputFile && !parsedArgs.outputDir) {
        LogFailedSema (L"Output parameter is mandatory for profiling!");

        return false;
    }

    pArgumentsOut->outputIsFile = parsedArgs.outputFile;
    pArgumentsOut->output = PathExpandEnvVars (parsedArgs.outputValue);

    if (!PathValid (pArgumentsOut->output)) {
        LogFailedSema (L"Output path is invalid!");

        return false;
    }

    std::wstring outDirectory = pArgumentsOut->outputIsFile ?
        PathGetDirectory (pArgumentsOut->output) : pArgumentsOut->output;
    if (!PathExists (outDirectory)) {
        LogFailedSema (L"Output directory does not exist!");
        
        return false;
    }

    // If we need to output to a file, perform additional checks
    if (pArgumentsOut->outputIsFile) {
        // If the file exists, check that it's indeed a file
        //   (and not something else)
        if (PathExists(pArgumentsOut->output) &&
            !IsFile(pArgumentsOut->output))
        {
            LogFailedSema (L"Output file exists, but it's not a file!");

            return false;
        }

        ETWP_ASSERT (pArgumentsOut->compressionMode != ApplicationArguments::CompressionMode::Invalid);

        // If it does not have an extension, append the appropriate one, if it does, check it
        std::wstring extension = PathGetExtension (pArgumentsOut->output);
        if (extension.empty ()) {
            pArgumentsOut->output += pArgumentsOut->compressionMode ==
                ApplicationArguments::CompressionMode::SevenZip ? L".7z" : L".etl";
        } else {
            if (pArgumentsOut->compressionMode == ApplicationArguments::CompressionMode::SevenZip)
            {
                if (_wcsicmp (extension.c_str (), L".7z") != 0) {
                    LogFailedSema (L"Invalid output path (expected file with .7z extension)!");

                    return false;
                }
            } else {
                if (_wcsicmp (extension.c_str (), L".etl") != 0) {
                    LogFailedSema (L"Invalid output path (expected file with .etl extension)!");

                    return false;
                }
            }
        }
    } else {    // Output is a directory
        if (!PathExists (pArgumentsOut->output)) {
            LogFailedSema (L"Output directory does not exist!");

            return false;
        }

        // If the path is a directory, but it does not end in a slash, add it
        //   here (to make potential concatenation easier later)
        if (pArgumentsOut->output.back () != L'\\' &&
            pArgumentsOut->output.back () != L'/')
        {
            pArgumentsOut->output += L'\\';
        }
    }

    return true;
}

bool SemaTarget (const ApplicationRawArguments& parsedArgs, ApplicationArguments* pArgumentsOut)
{
    if (!parsedArgs.target) {
        LogFailedSema (L"Target parameter is mandatory for profiling!");

        return false;
    }

    // Find out whether the input'd target is a PID or an exe name
    DWORD result = static_cast<DWORD> (_wtoi64 (parsedArgs.targetValue.c_str ()));
    // It's either zero, or invalid. Zero is an invalid PID anyways, so no problem
    if (result == 0) {
        // In case of emulate mode, only PID targeting is valid
        if (parsedArgs.emulate) {
            LogFailedSema (L"Target parameter must be PID if using \"emulate mode\"!");

            return false;
        }

        pArgumentsOut->targetName = parsedArgs.targetValue;
    } else {
        pArgumentsOut->targetIsPID = true;
        pArgumentsOut->targetPID = result;
    }

    return true;
}

bool SemaInputETLPath (const ApplicationRawArguments& parsedArgs, ApplicationArguments* pArgumentsOut)
{
    ETWP_ASSERT (parsedArgs.emulate);
    
    // The target value should contain an existing ETL file
    if (!PathExists (parsedArgs.inputETLPath)) {
        LogFailedSema (L"Input ETL file does not exist!");

        return false;
    }

    if (!IsFile (parsedArgs.inputETLPath)) {
        LogFailedSema (L"Input ETL path is not a file!");

        return false;
    }

    if (_wcsicmp (PathGetExtension (parsedArgs.inputETLPath).c_str (), L".ETL") != 0) {
        LogFailedSema (L"Input ETL file does not seem like an ETL file!");

        return false;
    }

    pArgumentsOut->inputETLPath = parsedArgs.inputETLPath;

    return true;
}

bool SemaCompressionMode (const ApplicationRawArguments& parsedArgs, ApplicationArguments* pArgumentsOut)
{
    if (!parsedArgs.compressionMode.empty ()) {
        if (parsedArgs.compressionMode == L"off") {
            pArgumentsOut->compressionMode = ApplicationArguments::CompressionMode::Off;
        } else if (parsedArgs.compressionMode == L"etw") {
            pArgumentsOut->compressionMode = ApplicationArguments::CompressionMode::ETW;
        } else if (parsedArgs.compressionMode == L"7z") {
            pArgumentsOut->compressionMode = ApplicationArguments::CompressionMode::SevenZip;
        } else {
            LogFailedSema (L"Invalid compression mode!");

            return false;
        }
    } else {
        pArgumentsOut->compressionMode = ApplicationArguments::CompressionMode::ETW;
    }

    return true;
}

bool SemaSamplingRate (const ApplicationRawArguments& parsedArgs, ApplicationArguments* pArgumentsOut)
{
    if (pArgumentsOut->emulate && parsedArgs.samplingRate) {
        LogFailedSema (L"Sampling rate is invalid in emulate mode!");

        return false;
    }

    if (parsedArgs.samplingRate) {
        pArgumentsOut->samplingRate = _wtoi (parsedArgs.samplingRateValue.c_str ());

        // We can't distinguish between a _wtoi error or a "legit" 0
        // It's not a problem here, because 0 is invalid anyways
        if (pArgumentsOut->samplingRate == 0    ||
            pArgumentsOut->samplingRate < 100   ||
            pArgumentsOut->samplingRate > 20'000)
        {
            LogFailedSema (L"Invalid sampling rate!");

            return false;
        }
    } else {
        // No sampling rate given. Use default (invalid aka. do not set)
        pArgumentsOut->samplingRate = 0;
    }

    return true;
}

bool SemaMinidump (const ApplicationRawArguments& parsedArgs, ApplicationArguments* pArgumentsOut)
{
    if (pArgumentsOut->emulate) {
        if (parsedArgs.minidump) {
            LogFailedSema (L"Minidump parameter is invalid in emulate mode!");

            return false;
        }

        if (parsedArgs.minidumpFlags) {
            LogFailedSema (L"Minidump flags parameter is invalid in emulate mode!");

            return false;
        }
    }

    if (!parsedArgs.minidump && parsedArgs.minidumpFlags) {
        LogFailedSema (L"Minidump flags parameter requires --mdump/-m!");

        return false;
    }

    if (parsedArgs.minidumpFlags) {
        // Unfortunately, zero is also a valid value :/
        pArgumentsOut->minidumpFlags = static_cast<uint32_t> (_wcstoui64 (parsedArgs.minidumpFlagsValue.c_str (),
                                                                          nullptr,
                                                                          16));
    }

    return true;
}

enum class UserProviderInfoVersion {    // I expect the format to be evolving, but I'd also like backward compatibility
    One,
    Unknown
};

UserProviderInfoVersion DetermineUserProviderInfoVersion (const std::wstring& /*string*/)
{
    return UserProviderInfoVersion::One;    // Only one version as of now...
}

bool SemaProviderInfoPart (const std::wstring& part, ApplicationArguments::UserProviderInfo* pProviderInfoOut)
{
    if (part.empty ()) {
        LogFailedSema (L"Invalid user provider descriptor (empty part encountered)!");

        return false;
    }

    // Split the part by colons
    std::vector<std::wstring> elements = SplitString (part, L':');
    std::wstring& providerID = elements[0];
    if (providerID.empty ()) {
        LogFailedSema (L"User provider name/GUID cannot be empty!");

        return false;
    }

    // Determine the type of provider ID, validate, etc.
    // Also check if the provider is registered. The logic we follow here is the same as xperf's (which is quite
    //   intuitive BTW):
    //   - in case of a registered name, the provider *must* be registered
    //   - in case of a dynamic name (*whatever), the provider is most likely a TraceLogging provider (not registered)
    //   - in case of a GUID, the provider might be either registered or not (e.g. TraceLogging provider)
    ApplicationArguments::UserProviderInfo result;
    if (IsGUID (providerID)) {
        result.type = ApplicationArguments::UserProviderInfo::GUID;
        if (ETWP_ERROR (!StringToGUID (providerID, &result.guid))) {
            LogFailedSema (L"Invalid user provider GUID!");

            return false;
        }

        // Try to look up a registered name for this GUID
        std::wstring providerName;
        GetNameOfRegisteredProvider (result.guid, &providerName);
        if (!providerName.empty ()) {
            Log (LogSeverity::Info, L"Found registered name for GUID \"" + providerID + L"\": \"" + providerName + L"\"");

            result.name = providerName;
        }

        std::wstring logString = L"Provider \"" + providerID + L"\" is ";
        if (!IsProviderRegistered (result.guid))
            logString += L"not ";

        logString += L"registered";

        Log (LogSeverity::Info, logString);
    } else if (providerID[0] == L'*') {
        result.type = ApplicationArguments::UserProviderInfo::DynamicName;

        providerID.erase (0, 1);
        result.name = providerID;

        if (ETWP_ERROR (!InferGUIDFromProviderName (result.name, &result.guid))) {
            LogFailedSema (L"Unable to infer provider GUID from name \"" + result.name + L"\"!");

            return false;
        }

        std::wstring guidString;
        if (GUIDToString (result.guid, &guidString))
            Log (LogSeverity::Info, L"GUID inferred from name \"" + result.name + L"\": " + guidString);

        std::wstring logString = L"Provider with dynamic name \"" + result.name + L"\" is ";
        if (!IsProviderRegistered (result.guid))
            logString += L"not ";

        logString += L"registered";

        Log (LogSeverity::Info, logString);
    } else {
        result.type = ApplicationArguments::UserProviderInfo::RegisteredName;
        result.name = providerID;

        if (!IsProviderRegistered (providerID)) {
            LogFailedSema (L"Provider \"" + result.name + L"\" is not registered!");

            return false;
        }

        if (ETWP_ERROR (!GetGUIDOfRegisteredProvider (result.name, &result.guid))) {
            LogFailedSema (L"Unable to query GUID of registered provider \"" + result.name + L"\"!");

            return false;
        }

        // A bit redundant because "registered names" *must* be registered
        Log (LogSeverity::Info, L"Provider \"" + result.name + L"\" is registered");
    }

    const size_t size = elements.size ();
    if (size > 3) { // There must be to colons at maximum
        LogFailedSema (L"Too many colons in user provider descriptor!");

        return false;
    }

    if (size >= 2) { // At least one colon -> provider name + keyword bitmask might be present
        const std::wstring& keywordBitmaskStr = elements[1];
        if (!keywordBitmaskStr.empty ()) {
            uint64_t keywordBitmask;
            if (keywordBitmaskStr[0] == L'~')
                keywordBitmask = ~_wcstoui64 (keywordBitmaskStr.substr (1).c_str (), nullptr, 0);
            else
                keywordBitmask = _wcstoui64 (keywordBitmaskStr.c_str (), nullptr, 0);

            if (keywordBitmask == 0) {
                LogFailedSema (L"Invalid Keyword bitmask (" + keywordBitmaskStr + L")!");

                return false;
            }

            result.keywordBitmask = keywordBitmask;
        }
    }

    if (size == 3) { // Two colons -> provider name + keyword bitmask + max. level, and potentially 'stack' at the end
        // Do we have 'stack' at the end?
        std::vector<std::wstring> trailingParts = SplitString (elements[2], L'\'');
        if (trailingParts.size () != 3 && trailingParts.size () != 1) {   // e.g. NOT like 0x3'stack' and NOT like 0x3
            LogFailedSema (L"Invalid user provider descriptor trailing \"" + elements[2] + L"\"!");

            return false;
        }

        if (trailingParts.size () >= 1) {   // e.g. 0x3, or 0x3'stack'
            const std::wstring& maxLevelStr = trailingParts[0];
            if (!maxLevelStr.empty ()) {
                unsigned long maxLevel = wcstoul (maxLevelStr.c_str (), nullptr, 0);
                if (maxLevel == 0) {
                    LogFailedSema (L"Invalid max. Level (" + maxLevelStr + L")!");

                    return false;
                }

                result.maxLevel = static_cast<UCHAR> (maxLevel);
            }
        }

        if (trailingParts.size () == 3) {    // e.g. 0x3'stack'
            if (trailingParts[1] != L"stack") {
                LogFailedSema (L"Invalid string in user provider descriptor trailing \"" + trailingParts[1] + L"\"!");

                return false;
            }

            result.stack = true;
        }
    }

    *pProviderInfoOut = result;

    return true;
}

bool ChekForDuplicateProviders (const std::vector<ApplicationArguments::UserProviderInfo>& providerInfos)
{
    std::vector<GUID> guids;
    for (auto&& providerInfo : providerInfos) {
        // Ugly, inefficient (O(n^2)) way, but doesn't require us to either implement operator< for GUIDs, or introduce
        //   our own GUID class
        for (auto&& guid : guids) {
            if (providerInfo.guid == guid) {  // Duplicate found
                std::wstring providerID;
                if (providerInfo.name.empty ())
                    ETWP_VERIFY (GUIDToString (providerInfo.guid, &providerID));
                else
                    providerID = providerInfo.name;

                LogFailedSema (L"Provider \"" + providerID + L"\" is specified more than once!");

                return false;
            }
        }

        guids.push_back (providerInfo.guid);
    }

    return true;
}

bool SemaUserProviderInfosVersionOne (const ApplicationRawArguments& parsedArgs, ApplicationArguments* pArgumentsOut)
{
    // Split string by '+' characters (multiple "entries" are separated with plus signs)
    std::vector<std::wstring> parts = SplitString (parsedArgs.userProvidersValue, L'+');

    // Some valid examples:
    //   - SomeProvider
    //   - SomeProvider::
    //   - SomeProvider::'stack'
    //   - 401AD6CA-8540-4340-8F1C-3BF0A736437D:~0x4:'stack'
    //   - *FunkyName::0x3'stack'
    for (auto&& part : parts) {
        ApplicationArguments::UserProviderInfo providerInfo;
        if (SemaProviderInfoPart (part, &providerInfo))
            pArgumentsOut->userProviderInfos.push_back (providerInfo);
        else
            return false;
    }

    // After we have finished analyzing all user provider descriptors, let's see if there are more than one descriptor
    //   referring to the same provider (illegal)
    return ChekForDuplicateProviders (pArgumentsOut->userProviderInfos);
}

bool SemaUserProviderInfos (const ApplicationRawArguments& parsedArgs, ApplicationArguments* pArgumentsOut)
{
    if (!parsedArgs.userProviders)
        return true;

    // User providers are supported on Win8+ only
    if (GetWinVersion () < BaseWinVersion::Win8) {
        Log (LogSeverity::Error,
             L"User providers are supported on Windows 8 and later only! Events from these providers will not be collected!");

        return false;
    }

    switch (DetermineUserProviderInfoVersion (parsedArgs.userProvidersValue)) {
        case UserProviderInfoVersion::One:
            return SemaUserProviderInfosVersionOne (parsedArgs, pArgumentsOut);
        default:
            ETWP_DEBUG_BREAK_STR (L"Invalid UserProviderInfoVersion!");

            return false;
    }
}

bool SemaStackCache (const ApplicationRawArguments& parsedArgs, ApplicationArguments* pArgumentsOut)
{
	if (!parsedArgs.stackCache)
		return true;

	if (pArgumentsOut->emulate) {
		LogFailedSema (L"ETW stack caching parameter is invalid in emulate mode!");

		return false;
	}

	if (GetWinVersion () < BaseWinVersion::Win8) {
        Log (LogSeverity::Error, L"ETW stack caching is supported on Windows 8 and later only!");

		return false;
	}

    return true;
}

bool UnpackRespFiles (const std::vector<std::wstring>& arguments, std::vector<std::wstring>* pArgumentsOut)
{
    std::vector<std::wstring> result = arguments;
    for (std::vector<std::wstring>::size_type i = 0; i < result.size (); ++i) {
        if (IsRespFileDirective (result[i])) {
            std::wstring respFilePath = GetRespFilePathFromDirective (result[i]);

            Log (LogSeverity::Debug, L"Processing response file: " + respFilePath);

            std::vector<std::wstring> respFileArgs;
            try {
                ResponseFile respFile (respFilePath);

                Log (LogSeverity::Debug, L"Detected encoding for response file \"" + respFilePath +
                    L"\" is: " + EncodingToString (respFile.GetEncoding ()));

                std::wstring error;
                if (!respFile.Unpack (&respFileArgs, &error)) {
                    LogFailedParse (error);

                    return false;
                }
            } catch (const ResponseFile::InitException& e) {
                LogFailedParse (e.GetMsg ());

                return false;
            }

            // Guard against "response file in response file" situation...
            for (auto it = respFileArgs.cbegin (); it != respFileArgs.cend (); ++it) {
                if (IsRespFileDirective (*it)) {
                    LogFailedParse (L"Illegal nested response file directive (" + respFilePath + L")!");

                    return false;
                }
            }

            result.erase (result.begin () + i); // Far from efficient, but doesn't matter here...
            result.insert (result.begin () + i, respFileArgs.begin (), respFileArgs.end ());
        }
    }

    *pArgumentsOut = std::move (result);

    return true;
}

}   // namespace

bool ParseArguments (const std::vector<std::wstring>& arguments, ApplicationRawArguments* pArgumentsOut)
{
    ETWP_ASSERT (pArgumentsOut != nullptr);

    ScopeLogger logger (LogSeverity::Debug, L"Parsing arguments", L"Finished parsing arguments");

    // Handle response files (if any)
    std::vector<std::wstring> finalArguments;
    if (!UnpackRespFiles (arguments, &finalArguments))
        return false;

    auto it = finalArguments.cbegin ();
    ++it; // The first argument is expected to be the application path; skip it

    if (it == finalArguments.cend () ||
        (finalArguments.size () == 2 && *it == L"--noaction"))   // noaction is "hidden", treat it accordingly
    {
        LogFailedParse (L"Not enough arguments!");

        return false;
    }

    // The command (if any) should be the first parameter
    if (IsCommand (*it)) {
        Log (LogSeverity::Debug, L"Parsing command: " + *it);

        // Right now the only command is 'profile'
        if (*it != L"profile") {
            LogFailedParse (L"Unknown command!", *it);

            return false;   // Unknown command
        } else {
            pArgumentsOut->profile = true;
            ++it;
        }
    }

    // Parse arguments
    for (;it != finalArguments.cend (); ++it) {
        Log (LogSeverity::Debug, L"Parsing argument: " + *it);

        if (*it == L"-") {
            LogFailedParse(L"\"-\" is not a valid argument!", *it);

            return false;
        }

        if (IsCommand (*it)) {
            LogFailedParse (L"Command encountered (only arguments are expected at this point)!", *it);

            return false;
        }

        if (IsShortArg (*it)) {
            if (!ParseShortArg (*it, pArgumentsOut))
                return false;
        } else {
            if (!ParseLongArg (*it, pArgumentsOut))
                return false;
        }
    }

    return true;
}

bool SemaArguments (const ApplicationRawArguments& parsedArgs, ApplicationArguments* pArgumentsOut)
{
    ETWP_ASSERT (pArgumentsOut != nullptr);

    ScopeLogger logger (LogSeverity::Debug, L"Analyzing arguments", L"Finished analyzing arguments");

    pArgumentsOut->profile = parsedArgs.profile;
    pArgumentsOut->noLogo = parsedArgs.noLogo;
    pArgumentsOut->verbose = parsedArgs.verbose;
    pArgumentsOut->help = parsedArgs.help;
    pArgumentsOut->debug = parsedArgs.debug;
    pArgumentsOut->emulate = parsedArgs.emulate;
    pArgumentsOut->cswitch = parsedArgs.cswitch;
    pArgumentsOut->minidump = parsedArgs.minidump;
    pArgumentsOut->stackCache = parsedArgs.stackCache;
    pArgumentsOut->noAction = parsedArgs.noAction;

    // We could check here if both --debug and --verbose was provided, but I don't think we need to be that nitpicky

    // Note: arguments might depend on other arguments being already sema'd. Keep this in mind when changing things
    //   around

    // If profile command is given, check needed/unnecessary params
    if (pArgumentsOut->profile) {
        if (pArgumentsOut->emulate) {
            if (!SemaInputETLPath (parsedArgs, pArgumentsOut))
                return false;
        }

        if (!SemaTarget (parsedArgs, pArgumentsOut))
            return false;

        if (!SemaCompressionMode (parsedArgs, pArgumentsOut))
            return false;

        if (!SemaOutputPath (parsedArgs, pArgumentsOut))
            return false;

        if (!SemaSamplingRate (parsedArgs, pArgumentsOut))
            return false;

        if (!SemaMinidump (parsedArgs, pArgumentsOut))
            return false;

        if (!SemaUserProviderInfos (parsedArgs, pArgumentsOut))
            return false;

		if (!SemaStackCache (parsedArgs, pArgumentsOut))
			return false;
    } else {    // Not profiling
        if (parsedArgs.target) {
            LogFailedSema (L"Target parameter is only valid for profiling!");

            return false;
        }

        if (parsedArgs.outputFile || parsedArgs.outputDir) {
            LogFailedSema (L"Output parameter is only valid for profiling!");

            return false;
        }

        if (parsedArgs.samplingRate) {
            LogFailedSema (L"Sampling rate parameter is only valid for profiling!");

            return false;
        }

        if (parsedArgs.emulate) {
            LogFailedSema (L"Emulate mode parameter is only valid for profiling!");

            return false;
        }

        if (parsedArgs.cswitch) {
            LogFailedSema (L"Context switch recording parameter is only valid for profiling!");

            return false;
        }

        if (parsedArgs.minidump) {
            LogFailedSema (L"Minidump parameter is only valid for profiling!");

            return false;
        }

        if (parsedArgs.minidumpFlags) {
            LogFailedSema (L"Minidump flags parameter is only valid for profiling!");

            return false;
        }

        if (parsedArgs.userProviders) {
            LogFailedSema (L"User providers info parameter is only valid for profiling!");

            return false;
        }

		if (parsedArgs.stackCache) {
			LogFailedSema (L"Stack cache parameter is only valid for profiling!");

			return false;
		}
    }

    return true;
}

}   // namespace ETWP