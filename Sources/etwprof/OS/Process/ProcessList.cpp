#include "ProcessList.hpp"

#include <TlHelp32.h>

#include "Log/Logging.hpp"
#include "OS/FileSystem/Utility.hpp"
#include "Utility/Asserts.hpp"

namespace ETWP {

ProcessList::InitException::InitException (const std::string& message):
    std::runtime_error (message)
{
}

ProcessList::ProcessList ()
{
    HANDLE hProcessSnaphot = CreateToolhelp32Snapshot (TH32CS_SNAPPROCESS, 0);
    if (hProcessSnaphot == INVALID_HANDLE_VALUE)
        throw InitException ("CreateToolhelp32Snapshot returned"
                             "INVALID_HANDLE_VALUE!");

    // Enumerate snapshot, populate container
    PROCESSENTRY32W currentProcess;
    currentProcess.dwSize = sizeof currentProcess;
    if (!Process32FirstW (hProcessSnaphot, &currentProcess)) {
        CloseHandle (hProcessSnaphot);

        throw InitException ("Process32FirstW returned FALSE");
    }

    uint16_t processCount = 0;
    do {
        m_processes.
            emplace_back (PathGetFileNameAndExtension (currentProcess.szExeFile),
                                  currentProcess.th32ProcessID);

        ++processCount;
    } while (Process32NextW (hProcessSnaphot, &currentProcess));

    Log (LogSeverity::Debug, L"There are " + std::to_wstring (processCount) +
         L" processes running on the system");

    CloseHandle (hProcessSnaphot);
}

bool ProcessList::Contains (const std::wstring& processName) const
{
    for (const auto& p : m_processes) {
        if (_wcsicmp (processName.c_str (), p.name.c_str ()) == 0)
            return true;
    }

    return false;
}

bool ProcessList::Contains (DWORD PID) const
{
    for (const auto& p : m_processes) {
        if (p.PID == PID)
            return true;
    }

    return false;
}

DWORD ProcessList::GetPID (const std::wstring& processName) const
{
    ETWP_ASSERT (GetCount (processName) == 1);

    for (const auto& p : m_processes) {
        if (_wcsicmp (processName.c_str (), p.name.c_str ()) == 0)
            return p.PID;
    }

    return 0;
}

bool ProcessList::GetName (DWORD PID, std::wstring* pNameOut) const
{
    for (const auto& p : m_processes) {
        if (p.PID == PID) {
            *pNameOut = p.name;

            return true;
        }
    }

    return false;
}

uint32_t ProcessList::GetCount (const std::wstring& processName) const
{
    uint32_t count = 0;

    for (const auto& p : m_processes) {
        if (_wcsicmp (processName.c_str (), p.name.c_str ()) == 0)
            ++count;
    }

    return count;
}

std::vector<ProcessList::Process> ProcessList::GetAll (const std::wstring& processName) const
{
    std::vector<ProcessList::Process> result;

    for (const auto& p : m_processes) {
        if (_wcsicmp (processName.c_str (), p.name.c_str ()) == 0)
            result.push_back (p);
    }

    return result;
}

}   // namespace ETWP