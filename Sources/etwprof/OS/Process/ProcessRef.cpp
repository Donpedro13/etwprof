#include "ProcessRef.hpp"

#include <optional>

#include "Utility/Asserts.hpp"
#include "Utility/OnExit.hpp"

#include "OS/FileSystem/Utility.hpp"
#include "OS/Utility/Win32Utils.hpp"

namespace ETWP {

namespace {

int ProcessOptionsToWin32Flags (ProcessRef::AccessOptions options)
{
    int result = PROCESS_QUERY_INFORMATION; // Implied
    if (IsFlagSet (options, ProcessRef::AccessOptions::Terminate))
        result |= PROCESS_TERMINATE;

    if (IsFlagSet (options, ProcessRef::AccessOptions::Synchronize))
        result |= SYNCHRONIZE;

    if (IsFlagSet (options, ProcessRef::AccessOptions::ReadMemory))
        result |= PROCESS_VM_READ;

    return result;
}

Result<std::pair<HANDLE, HANDLE>> CreateProcessImpl (std::optional<std::wstring> processPath,
                                                     const std::wstring& fullCommandLine,
                                                     ProcessRef::CreateOptions createOptions,
                                                     bool suspended)
{
    std::unique_ptr<WCHAR[]> cmdLineCopy (new WCHAR[fullCommandLine.length () + 1]);
    if (wcscpy_s (cmdLineCopy.get (),
                  fullCommandLine.length () + 1,
                  fullCommandLine.c_str ()) != 0)
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

    if (suspended)
        createFlags |= CREATE_SUSPENDED;

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

    OnExit devNullHandleCloser ([&hDevNull] () { if (hDevNull != INVALID_HANDLE_VALUE) ETWP_VERIFY (CloseHandle (hDevNull)); });

    if (CreateProcessW (processPath.has_value () ? processPath->c_str () : nullptr,
                        cmdLineCopy.get (),
                        nullptr,
                        nullptr,
                        IsFlagSet (createOptions, ProcessRef::CreateOptions::NoOutput) ? TRUE : FALSE,
                        createFlags,
                        nullptr,
                        nullptr,
                        &startupInfo,
                        &processInfo) == FALSE)
    {
        return Error (L"CreateProcessW failed!");
    }

    return std::make_pair (processInfo.hProcess , processInfo.hThread);
}

Result<std::pair<HANDLE, HANDLE>> StartProcessImpl (const std::wstring& processPath,
                                                    const std::wstring& args,
                                                    ProcessRef::CreateOptions createOptions,
                                                    bool suspended)
{
    const std::wstring commandLine = L"\"" + processPath + L"\" " + args;

    return CreateProcessImpl (processPath, commandLine, createOptions, suspended);
}

Result<std::pair<HANDLE, HANDLE>> StartProcessImpl (const std::wstring& commandLine,
                                                    ProcessRef::CreateOptions createOptions,
                                                    bool suspended)
{
    return CreateProcessImpl (std::nullopt, commandLine, createOptions, suspended);
}

Result<std::wstring> GetImageNameFromProcessHandle (HANDLE hProcess)
{
    WCHAR imageName[MAX_PATH] = {};
    DWORD bufSize = MAX_PATH;

    if (!QueryFullProcessImageNameW (hProcess, PROCESS_NAME_NATIVE, imageName, &bufSize))
        return Error (L"QueryFullProcessImageName failed");

    return PathGetFileNameAndExtension (imageName);
}

}	// namespace

ProcessRef::InitException::InitException (const std::wstring& msg): Exception (msg)
{
}

Result<ProcessRef> ProcessRef::StartProcess (const std::wstring& processPath,
                                             const std::wstring& args,
                                             ProcessRef::AccessOptions accessOptions,
                                             ProcessRef::CreateOptions createOptions)
{
    auto createResult = StartProcessImpl (processPath, args, createOptions, false);
    if (!createResult.has_value ())
        return Error (createResult.error ());

    auto[hProcess, hMainThread] = *createResult;
    ETWP_VERIFY (CloseHandle (hMainThread));

    try {
        return ProcessRef (hProcess, accessOptions);
    } catch (const Exception& e) {
        return Error (e.GetMsg ());
    }
}


Result<ProcessRef> ProcessRef::StartProcess (const std::wstring& commandLine,
                                             ProcessRef::AccessOptions accessOptions,
                                             ProcessRef::CreateOptions createOptions)
{
    auto createResult = StartProcessImpl (commandLine, createOptions, false);
    if (!createResult.has_value ())
        return Error (createResult.error ());

    auto[hProcess, hMainThread] = *createResult;
    ETWP_VERIFY (CloseHandle (hMainThread));

    try {
        return ProcessRef (hProcess, accessOptions);
    }
    catch (const Exception& e) {
        return Error (e.GetMsg ());
    }
}

ProcessRef::ProcessRef (PID pid, AccessOptions options):
    m_info (pid, std::wstring ()),
    m_handle (OpenProcess (ProcessOptionsToWin32Flags (options), FALSE, pid)),
    m_options (options)
{
    if (m_handle == nullptr)
        throw InitException (L"Unable to open process HANDLE (OpenProcess failed)!");

    Result<std::wstring> imageNameResult = GetImageNameFromProcessHandle (m_handle);
    if (!imageNameResult.has_value ())
        throw InitException (imageNameResult.error ());
    else
        m_info.imageName = *imageNameResult;
}

ProcessRef::ProcessRef (ProcessRef&& other):
    m_info (other.m_info),
    m_handle (other.m_handle),
    m_options (other.m_options)
{
    other.m_handle = nullptr;
}

ProcessRef::~ProcessRef ()
{
    if (m_handle != nullptr)
        ETWP_VERIFY (CloseHandle (m_handle));
}

bool ProcessRef::Wait (uint32_t timeout/* = INFINITE*/) const
{
    ETWP_ASSERT (IsFlagSet (m_options, AccessOptions::Synchronize));

    switch (Win32::WaitForObject (m_handle, timeout)) {
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

bool ProcessRef::Terminate (uint32_t exitCode) const
{
    ETWP_ASSERT (IsFlagSet (m_options, AccessOptions::Terminate));

    return TerminateProcess (m_handle, exitCode);
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
    return m_info.pid;
}

std::wstring ProcessRef::GetName () const
{
    return m_info.imageName;
}

HANDLE ProcessRef::GetHandle () const
{
    return m_handle;
}

ProcessRef::AccessOptions ProcessRef::GetOptions () const
{
    return m_options;
}

ProcessRef::ProcessRef (HANDLE hProcess, AccessOptions accessOptions):
    m_info (GetProcessId (hProcess), {}),
    m_handle (hProcess),
    m_options (accessOptions)
{
    ETWP_ASSERT (hProcess != nullptr);
    
    if (m_info.pid == 0)
        throw InitException (L"GetProcessId failed");

    Result<std::wstring> imageNameResult = GetImageNameFromProcessHandle (m_handle);
    if (!imageNameResult.has_value ())
        throw InitException (imageNameResult.error ());
    else
        m_info.imageName = *imageNameResult;
}

Result<SuspendedProcessRef> ProcessRef::StartProcessSuspended (const std::wstring& processPath,
                                                               const std::wstring& args,
                                                               AccessOptions accessOptions,
                                                               CreateOptions createOptions)
{
    auto createResult = StartProcessImpl (processPath, args, createOptions, true);
    if (!createResult.has_value ())
        return Error (createResult.error ());

    auto [hProcess, hMainThread] = *createResult;

    try {
        return SuspendedProcessRef (hProcess, hMainThread, accessOptions);
    } catch (const Exception& e) {
        return Error (e.GetMsg ());
    }
}

Result<SuspendedProcessRef> ProcessRef::StartProcessSuspended (const std::wstring& commandLine,
                                                               AccessOptions accessOptions,
                                                               CreateOptions createOptions)
{
    auto createResult = StartProcessImpl (commandLine, createOptions, true);
    if (!createResult.has_value ())
        return Error (createResult.error ());

    auto [hProcess, hMainThread] = *createResult;

    
    try {
        return SuspendedProcessRef(hProcess, hMainThread, accessOptions);
    } catch (const Exception& e) {
        return Error (e.GetMsg ());
    }
}

ProcessRef SuspendedProcessRef::Resume (SuspendedProcessRef&& suspendedProcessRef)
{
    if (ResumeThread (suspendedProcessRef.m_hMainThread) == -1)
        throw ProcessRef::InitException (L"ResumeThread failed");

    ETWP_VERIFY (CloseHandle (suspendedProcessRef.m_hMainThread));

    suspendedProcessRef.m_hMainThread = nullptr;

    return std::move (suspendedProcessRef.m_processRef);
}

SuspendedProcessRef::SuspendedProcessRef (SuspendedProcessRef&& other) noexcept :
    m_processRef (std::move (other.m_processRef)),
    m_hMainThread (other.m_hMainThread)
{
    other.m_hMainThread = nullptr;
}

SuspendedProcessRef::~SuspendedProcessRef ()
{

    if (m_hMainThread != nullptr)
        ETWP_VERIFY (CloseHandle (m_hMainThread));
}

SuspendedProcessRef::SuspendedProcessRef (HANDLE hProcess, HANDLE hMainThread, ProcessRef::AccessOptions accessOptions):
    m_processRef (hProcess, accessOptions),
    m_hMainThread (hMainThread)
{
}

PID SuspendedProcessRef::GetPID () const
{
    return m_processRef.GetPID ();
}

std::wstring SuspendedProcessRef::GetName () const
{
    return m_processRef.GetName ();
}

bool SuspendedProcessRef::Terminate (uint32_t exitCode) const
{
    return m_processRef.Terminate (exitCode);
}

}   // namespace ETWP