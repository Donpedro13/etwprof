#include "Arguments.hpp"

#include "Application.hpp"

#include "Log/Logging.hpp"

#include "OS/FileSystem/Utility.hpp"

#include "Utility/Asserts.hpp"
#include "Utility/ResponseFile.hpp"

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
    }

    LogFailedParse (L"Unknown value argument!", arg);

    return false;
}

bool ParseLongNonAssignmentArg (const std::wstring& arg, ApplicationRawArguments* pArgumentsOut)
{
    ETWP_ASSERT (!IsShortArg (arg));
    ETWP_ASSERT (!IsAssignmentArg (arg));

    std::wstring argName = GetArgName (arg);
    // --help ; --verbose ; --nologo ; --debug ; --cswitch ; --mdump
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
    if (it == finalArguments.cend ()) {
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
    }

    return true;
}

}   // namespace ETWP