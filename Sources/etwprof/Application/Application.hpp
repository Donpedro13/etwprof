#ifndef ETWP_APPLICATION_HPP
#define ETWP_APPLICATION_HPP

#include <memory>
#include <vector>
#include <string>

#include "Arguments.hpp"

#include "OS/Process/WaitableProcessGroup.hpp"
#include "OS/Version/WinVersion.hpp"

#include "Profiler/IETWBasedProfiler.hpp"

#include "Utility/Macros.hpp"
#include "Utility/Result.hpp"

namespace ETWP {

class Application final {
public:
    ETWP_DISABLE_COPY_AND_MOVE (Application);

    typedef std::vector<std::wstring> ArgumentVector;

    static Application& Instance ();

    bool Init (const ArgumentVector& arguments, const std::wstring& commandLine);

    int Run ();

private:
    ApplicationArguments m_args;

    std::unique_ptr<IETWBasedProfiler> m_pProfiler;

    Application ();
    ~Application ();

    // ConsoleControlHandler for profiling
    static BOOL WINAPI CtrlHandler (DWORD fdwCtrlType);

    bool CheckWinVersion () const;

    void PrintUsage () const;
    void PrintLogo () const;

    // Commands
    bool DoProfile ();

    // Helpers
    Result<std::unique_ptr<WaitableProcessGroup>> GetTargets () const;
    Result<SuspendedProcessRef>                   StartTarget () const;
        
    bool HandleArguments (const Application::ArgumentVector& arguments, const std::wstring& commandLine);
};

}   // namespace ETWP

#endif  // #ifndef ETWP_APPLICATION_HPP