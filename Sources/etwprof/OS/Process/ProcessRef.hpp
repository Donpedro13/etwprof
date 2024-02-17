#ifndef ETWP_PROCESS_HPP
#define ETWP_PROCESS_HPP

#include <windows.h>

#include <string>

#include "OS/Utility/OSTypes.hpp"

#include "Utility/Exception.hpp"

namespace ETWP {

// Class for wrapping an already existing, OS-native process. The process HANDLE is kept open until the object exists.
class ProcessRef final {
public:
    using Options = uint16_t;
    enum : Options {
        Default     = 0,
        Synchronize = 0b01,
        ReadMemory  = 0b10,
    };

    class InitException : public Exception {
    public:
        InitException (const std::wstring& msg);
        virtual ~InitException () = default;
    };

    ProcessRef (PID pid, Options options);
    ProcessRef (ProcessRef&&);
    ~ProcessRef();

    ProcessRef (const ProcessRef&) = delete;
    const ProcessRef& operator= (const ProcessRef&) = delete;

    bool Wait (uint32_t timeout = INFINITE) const;
    bool HasFinished () const;

    DWORD GetExitCode () const;

    PID          GetPID () const;
    std::wstring GetName () const;

    HANDLE  GetHandle () const;
    Options GetOptions () const;

private:
    PID    m_pid;
    HANDLE m_handle;

    std::wstring m_imageName;

    Options m_options;
};

}   // namespace ETWP

#endif  // #ifndef ETWP_PROCESS_HPP