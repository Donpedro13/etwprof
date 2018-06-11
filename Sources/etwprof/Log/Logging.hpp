#ifndef ETWP_LOG_HPP
#define ETWP_LOG_HPP

#include <string>

#include "Utility/Macros.hpp"

namespace ETWP {

enum class LogSeverity {
    Debug,
    Info,
    Warning,
    Error,

    LeastVerbose = Error,
    MostVerbose = Debug,
};

void Log (LogSeverity severity, const std::wstring& logLine);
void SetMinLogSeverity (LogSeverity newSeverity);
LogSeverity GetMinLogSeverity ();

class ScopeLogger final {
public:
    ETWP_DISABLE_COPY_AND_MOVE (ScopeLogger);

    ScopeLogger (LogSeverity severity,
                 const std::wstring& entryString,
                 const std::wstring& exitString);
    ~ScopeLogger ();

private:
    std::wstring m_exitString;
    LogSeverity  m_severity;
};

}   // namespace ETWP

#endif  // #ifndef ETWP_LOG_HPP