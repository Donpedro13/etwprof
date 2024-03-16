#include "ProcessRef.hpp"

#include "Utility/Asserts.hpp"
#include "Utility/OnExit.hpp"

#include "OS/FileSystem/Utility.hpp"
#include "OS/Utility/Win32Utils.hpp"

namespace ETWP {

namespace {

int ProcessOptionsToWin32Flags (ProcessRef::AccessOptions options)
{
    int result = PROCESS_QUERY_INFORMATION; // Implied
    if (IsFlagSet (options, ProcessRef::AccessOptions::Synchronize))
        result |= SYNCHRONIZE;

    if (IsFlagSet (options, ProcessRef::AccessOptions::ReadMemory))
        result |= PROCESS_VM_READ;

    return result;
}

// /analyze is unaware that we close the process handle in an OnExit object...
#pragma warning (push)
#pragma warning (disable: 6335)
Result<ProcessRef> CreateProcessImpl (const std::wstring& processPath,
                                      const std::wstring& args,
                                      ProcessRef::AccessOptions accessOptions,
                                      ProcessRef::CreateOptions createOptions)
{
    const std::wstring realArgs = processPath + L" " + args;
    std::unique_ptr<WCHAR[]> cmdLineCopy (new WCHAR[realArgs.length () + sizeof L'\0']);
    if (wcscpy_s (cmdLineCopy.get (),
                  realArgs.length () + 1,
                  realArgs.c_str ()) != 0)
    {
        return Error (L"Unable to construct command line!");
    }

    SECURITY_ATTRIBUTES secAttr = {};
    secAttr.nLength = sizeof secAttr;
    secAttr.lpSecurityDescriptor = nullptr;
    STARTUPINFOW startupInfo = {};
    startupInfo.cb = sizeof startupInfo;
    PROCESS_INFORMATION processInfo = {};

    HANDLE hDevNull = INVALID_HANDLE_VALUE;
    DWORD createFlags = 0;

    if (IsFlagSet (createOptions, ProcessRef::CreateOptions::NewConsole))
        createFlags |= CREATE_NEW_CONSOLE;

    if (IsFlagSet (createOptions, ProcessRef::CreateOptions::NoOutput)) {
        secAttr.bInheritHandle = TRUE;

        startupInfo.hStdOutput = hDevNull;
        startupInfo.hStdError = hDevNull;
        startupInfo.hStdInput = nullptr;
        startupInfo.dwFlags |= STARTF_USESTDHANDLES;

        hDevNull = CreateFileW (L"NUL",
                                GENERIC_WRITE,
                                0,
                                &secAttr,
                                OPEN_EXISTING,
                                0,
                                nullptr);

        if (hDevNull == INVALID_HANDLE_VALUE)
            return Error (L"Unable to get handle for NUL!");;
    }

    OnExit devNullHandleCloser ([&hDevNull] () { if (hDevNull != INVALID_HANDLE_VALUE) CloseHandle (hDevNull); });

    if (CreateProcessW (processPath.c_str (),
                        cmdLineCopy.get (),
                        nullptr,
                        nullptr,
                        TRUE,
                        createFlags,
                        nullptr,
                        nullptr,
                        &startupInfo,
                        &processInfo) == FALSE)
    {
        return Error (L"CreateProcessW failed!");
    }

    CloseHandle (processInfo.hThread);

    OnExit processHandleCloser ([&processInfo] () { CloseHandle (processInfo.hProcess); });

    return ProcessRef (processInfo.dwProcessId, accessOptions);
}
#pragma warning (pop)

}	// namespace

ProcessRef::InitException::InitException (const std::wstring& msg): Exception (msg)
{
}

Result<ProcessRef> ProcessRef::StartProcess (const std::wstring& processPath,
                                             const std::wstring& args,
                                             ProcessRef::AccessOptions accessOptions,
                                             ProcessRef::CreateOptions createOptions)
{
    return CreateProcessImpl (processPath, args, accessOptions, createOptions);
}

ProcessRef::ProcessRef (PID pid, AccessOptions options):
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
    ETWP_ASSERT (IsFlagSet (m_options, AccessOptions::Synchronize));

    switch (Win32::WaitForObject(m_handle, timeout)) {
        case Win32::WaitResult::Signaled:
            return true;

        case Win32::WaitResult::Timeout:
            return false;

        case Win32::WaitResult::Failed:
            [[fallthrough]];    // Shouldn't occur with this class

        default:
            ETWP_DEBUG_BREAK ();

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

std::wstring ProcessRef::GetName () const
{
    return m_imageName;
}

HANDLE ProcessRef::GetHandle () const
{
    return m_handle;
}

ProcessRef::AccessOptions ProcessRef::GetOptions () const
{
    return m_options;
}

}   // namespace ETWP