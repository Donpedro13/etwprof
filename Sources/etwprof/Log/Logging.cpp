#include "Logging.hpp"

#include "OS/Stream/GlobalStreams.hpp"
#include "OS/Stream/OStreamManipulators.hpp"
#include "Utility/Asserts.hpp"

namespace ETWP {

namespace {

LogSeverity gMinLogSeverity = LogSeverity::Warning;

void LogError (const std::wstring& logLine)
{
    CErr () << ColorReset << BgColorRed << FgColorYellow << L"ERROR:" <<
        ColorReset << FgColorRed <<  L" " <<  logLine << ColorReset << Endl;
}

void LogWarning (const std::wstring& logLine)
{
    CErr () << ColorReset << FgColorYellow << L"WARNING: " << logLine <<
        ColorReset << Endl;
}

void LogInfo (const std::wstring& logLine)
{
    COut () << ColorReset << FgColorGray << L"LOG: " << logLine <<
        ColorReset << Endl;
}

void LogDebug (const std::wstring& logLine)
{
    COut () << ColorReset << FgColorDarkGreen << L"DEBUG:" << L" " <<
        logLine << ColorReset << Endl;
}

}   // namespace

void Log (LogSeverity severity, const std::wstring& logLine)
{
    if (severity < gMinLogSeverity)
        return;

    switch (severity) {
        case LogSeverity::Error:
            LogError (logLine);
            break;
        case LogSeverity::Warning:
            LogWarning (logLine);
            break;
        case LogSeverity::Info:
            LogInfo (logLine);
            break;
        case LogSeverity::Debug:
            LogDebug (logLine);
            break;
        default:
            ETWP_DEBUG_BREAK_STR (L"Invalid log severity!");
            return;
    }
}

void SetMinLogSeverity (LogSeverity newSeverity)
{
    gMinLogSeverity = newSeverity;
}

LogSeverity GetMinLogSeverity ()
{
    return gMinLogSeverity;
}

ScopeLogger::ScopeLogger (LogSeverity severity,
                          const std::wstring& entryString,
                          const std::wstring& exitString):
    m_exitString (exitString),
    m_severity (severity)
{
    Log (m_severity, entryString);
}

ScopeLogger::~ScopeLogger ()
{
    Log (m_severity, m_exitString);
}

}   // namespace ETWP