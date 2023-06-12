#include "ProcessRef.hpp"

#include "Utility/Asserts.hpp"
#include "OS/FileSystem/Utility.hpp"

#include <iostream>  // TODO remove

namespace ETWP {

namespace {

int ProcessOptionsToWin32Flags (ProcessRef::Options options)
{
    int result = PROCESS_QUERY_INFORMATION; // Implied
    if (options & ProcessRef::Options::Synchronize)
        result |= SYNCHRONIZE;

    if (options & ProcessRef::Options::ReadMemory)
        result |= PROCESS_VM_READ;

    return result;
}

}	// namespace

ProcessRef::InitException::InitException (const std::wstring& msg): Exception (msg)
{
}

ProcessRef::ProcessRef (PID pid, Options options):
    m_pid (pid),
    m_handle (OpenProcess (ProcessOptionsToWin32Flags (options), FALSE, pid)),
    m_imageName (),
    m_options (options)
{
    if (m_handle == nullptr)
        throw InitException (L"Unable to open process HANDLE (OpenProcess failed)!");

    WCHAR imageName[MAX_PATH] = {};
    DWORD bufSize = MAX_PATH;

    if (!QueryFullProcessImageNameW (m_handle, PROCESS_NAME_NATIVE, imageName, &bufSize)) {
        CloseHandle (m_handle);

        throw InitException (L"Unable to get image name of process (QueryFullProcessImageNameW failed)!");
    }

    m_imageName = PathGetFileNameAndExtension (imageName);
}

ProcessRef::ProcessRef (ProcessRef&& other):
    m_pid (other.m_pid),
    m_handle (other.m_handle),
    m_imageName (std::move (other.m_imageName)),
    m_options (other.m_options)
{
    other.m_pid = InvalidPID;
    other.m_handle = nullptr;
}

ProcessRef::~ProcessRef ()
{
    if (m_handle != nullptr)
        CloseHandle (m_handle);
}

bool ProcessRef::Wait (uint32_t timeout/* = INFINITE*/) const
{
    ETWP_ASSERT (m_options & Synchronize);

    switch (WaitForSingleObject (m_handle, timeout)) {
        case WAIT_OBJECT_0:
            return true;

        case WAIT_TIMEOUT:
            return false;

        case WAIT_FAILED:
            [[fallthrough]];
        default:
            ETWP_DEBUG_BREAK_STR (L"Impossible value returned from WaitForSingleObject in " __FUNCTIONW__  L"!");
            
            return false;
    }
}

bool ProcessRef::HasFinished () const
{
    return Wait (0);
}

DWORD ProcessRef::GetExitCode () const
{
    ETWP_ASSERT (HasFinished ());

    DWORD exitCode = MAXDWORD;
    ETWP_VERIFY (GetExitCodeProcess (m_handle, &exitCode));

    return exitCode;
}

PID ProcessRef::GetPID () const
{
    return m_pid;
}

HANDLE ProcessRef::GetHandle () const
{
    return m_handle;
}

ProcessRef::Options ProcessRef::GetOptions () const
{
    return m_options;
}

}   // namespace ETWP