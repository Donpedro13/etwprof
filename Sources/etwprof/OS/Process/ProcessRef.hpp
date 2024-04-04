#ifndef ETWP_PROCESS_HPP
#define ETWP_PROCESS_HPP

#include <windows.h>

#include <string>

#include "OS/Utility/OSTypes.hpp"

#include "Utility/Exception.hpp"
#include "Utility/EnumFlags.hpp"
#include "Utility/Result.hpp"

namespace ETWP {

struct ProcessInfo {
    PID          pid;
    std::wstring imageName;
};

// Class for wrapping an already existing, OS-native process. The process HANDLE is kept open until the object exists.
class ProcessRef final {
public:
    friend class SuspendedProcessRef;

    enum class AccessOptions {
        Default     = 0,
        Synchronize = 0b001,
        ReadMemory  = 0b010,
        Terminate   = 0b100
    };

    ETWP_ENUM_FLAG_SUPPORT_CLASS(AccessOptions);

    enum class CreateOptions {
        Default    = 0,
        NoOutput   = 0b01,
        NewConsole = 0b10,
    };

    ETWP_ENUM_FLAG_SUPPORT_CLASS(CreateOptions);

    class InitException : public Exception {
    public:
        InitException (const std::wstring& msg);
        virtual ~InitException () = default;
    };

    static Result<ProcessRef> StartProcess (const std::wstring& processPath,
                                            const std::wstring& args,
                                            AccessOptions accessOptions = AccessOptions::Default,
                                            CreateOptions createOptions = CreateOptions::Default);

    static Result<ProcessRef> StartProcess (const std::wstring& commandLine,
                                            AccessOptions accessOptions = AccessOptions::Default,
                                            CreateOptions createOptions = CreateOptions::Default);

    static Result<SuspendedProcessRef> StartProcessSuspended (const std::wstring& processPath,
                                                              const std::wstring& args,
                                                              AccessOptions accessOptions = AccessOptions::Default,
                                                              CreateOptions createOptions = CreateOptions::Default);

    static Result<SuspendedProcessRef> StartProcessSuspended (const std::wstring& commandLine,
                                                              AccessOptions accessOptions = AccessOptions::Default,
                                                              CreateOptions createOptions = CreateOptions::Default);

    ProcessRef (PID pid, AccessOptions options);
    ProcessRef (ProcessRef&&);
    ~ProcessRef ();

    ProcessRef (const ProcessRef&) = delete;
    const ProcessRef& operator= (const ProcessRef&) = delete;

    bool Wait (uint32_t timeout = INFINITE) const;
    bool HasFinished () const;

    bool Terminate (uint32_t exitCode) const;

    DWORD GetExitCode () const;

    PID          GetPID () const;
    std::wstring GetName () const;

    HANDLE        GetHandle () const;
    AccessOptions GetOptions () const;

private:
    ProcessRef (HANDLE hProcess, AccessOptions accessOptions);

    ProcessInfo m_info;

    HANDLE m_handle;

    AccessOptions m_options;
};

class SuspendedProcessRef final {
public:
    friend class ProcessRef;

    static ProcessRef Resume (SuspendedProcessRef&& suspendedProcessRef);

    SuspendedProcessRef (SuspendedProcessRef&& other) noexcept;
    ~SuspendedProcessRef ();

    PID          GetPID () const;
    std::wstring GetName () const;

    bool Terminate (uint32_t exitCode) const;

private:
    SuspendedProcessRef (HANDLE hProcess, HANDLE hMainThread, ProcessRef::AccessOptions accessOptions);

    ProcessRef m_processRef;
    HANDLE m_hMainThread;
};

}   // namespace ETWP

#endif  // #ifndef ETWP_PROCESS_HPP