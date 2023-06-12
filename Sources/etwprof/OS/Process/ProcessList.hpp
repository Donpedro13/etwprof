#ifndef ETWP_PROCESS_LIST_HPP
#define ETWP_PROCESS_LIST_HPP

#include <windows.h>

#include <string>
#include <vector>
#include <stdexcept>

namespace ETWP {

// Immutable snapshot of processes
class ProcessList final {
public:
    class InitException : public std::runtime_error {
    public:
        InitException (const std::string& message);
    };

    struct Process {
        std::wstring name;
        DWORD        PID;

        Process (const std::wstring& name, DWORD PID):
            name (name), PID (PID)
        {}

        Process (): name (L""), PID (0)
        {}
    };

    ProcessList (); // Throws InitException

    bool Contains (const std::wstring& processName) const;
    bool Contains (DWORD PID) const;

    // Returns 0 if not found
    DWORD GetPID (const std::wstring& processName) const;
    bool GetName (DWORD PID, std::wstring* pNameOut) const;

    uint32_t GetCount (const std::wstring& processName) const;

    std::vector<Process> GetAll (const std::wstring& processName) const;

private:
    using Container = std::vector<Process>;

    Container m_processes;
};

}   // namespace ETWP

#endif  // #ifndef ETWP_PROCESS_LIST_HPP