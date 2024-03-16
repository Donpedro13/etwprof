#ifndef ETWP_PROCESS_HPP
#define ETWP_PROCESS_HPP

#include <windows.h>

#include <string>

#include "OS/Utility/OSTypes.hpp"

#include "Utility/Exception.hpp"
#include "Utility/EnumFlags.hpp"
#include "Utility/Result.hpp"

namespace ETWP {

// Class for wrapping an already existing, OS-native process. The process HANDLE is kept open until the object exists.
class ProcessRef final {
public:
    enum class AccessOptions {
        Default = 0,
        Synchronize = 0b01,
        ReadMemory = 0b10,
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

    ProcessRef (PID pid, AccessOptions options);
    ProcessRef (ProcessRef&&);
    ~ProcessRef();

    ProcessRef (const ProcessRef&) = delete;
    const ProcessRef& operator= (const ProcessRef&) = delete;

    bool Wait (uint32_t timeout = INFINITE) const;
    bool HasFinished () const;

    DWORD GetExitCode () const;

    PID          GetPID () const;
    std::wstring GetName () const;

    HANDLE        GetHandle () const;
    AccessOptions GetOptions () const;

private:
    PID    m_pid;
    HANDLE m_handle;

    std::wstring m_imageName;

    AccessOptions m_options;
};

}   // namespace ETWP

#endif  // #ifndef ETWP_PROCESS_HPP