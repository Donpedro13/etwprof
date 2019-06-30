#ifndef ETWP_APPLICATION_HPP
#define ETWP_APPLICATION_HPP

#include <memory>
#include <vector>
#include <string>

#include "Arguments.hpp"

#include "OS/Process/ProcessList.hpp"
#include "OS/Version/WinVersion.hpp"

#include "Profiler/IETWBasedProfiler.hpp"

#include "Utility/Macros.hpp"

namespace ETWP {

class Application final {
public:
    ETWP_DISABLE_COPY_AND_MOVE (Application);

    typedef std::vector<std::wstring> ArgumentVector;

    static Application& Instance ();

    bool Init (const ArgumentVector& arguments);

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
    bool GetTarget (ProcessList::Process* pTargetOut) const;
    std::wstring GenerateDefaultOutputName (const ProcessList::Process& process,
                                            ApplicationArguments::CompressionMode compressionMode) const;
        
    bool HandleArguments (const Application::ArgumentVector& arguments);
};

}   // namespace ETWP

#endif  // #ifndef ETWP_APPLICATION_HPP