#include "Utility.hpp"

#include <windows.h>
#include <tlhelp32.h>

#include <iostream>
#include <filesystem>

namespace PTH {

WinHandle::WinHandle (HANDLE handle) : m_handle (handle)
{
}

WinHandle::WinHandle (WinHandle&& rhs):
    m_handle (rhs.m_handle)
{
    rhs.m_handle = nullptr;
}

WinHandle::~WinHandle ()
{
    if (m_handle != nullptr && m_handle != INVALID_HANDLE_VALUE)
        CloseHandle (m_handle);
}

HANDLE WinHandle::Get ()
{
    return m_handle;
}

[[noreturn]] void Fail (const std::wstring& msg)
{
    std::wcerr << L"ERROR: " << msg << std::endl;

    exit (EXIT_FAILURE);
}

std::wstring GetExePath ()
{
	WCHAR fullPathStr[MAX_PATH] = {};
	GetModuleFileNameW (nullptr, fullPathStr, MAX_PATH);

	return fullPathStr;
}

std::wstring GetExeFolderPath ()
{
	const std::wstring exePath = GetExePath ();

	return std::filesystem::path (exePath).remove_filename ();
}

bool GetParentPID (DWORD* pPIDOut)
{
    DWORD ownPID = GetCurrentProcessId ();

    HANDLE hSnapshot = CreateToolhelp32Snapshot (TH32CS_SNAPPROCESS, 0);
    if (hSnapshot == INVALID_HANDLE_VALUE)
        Fail (L"Unable to create process snapshot!");

    PROCESSENTRY32W processEntry;
    processEntry.dwSize = sizeof processEntry;

    if (!Process32FirstW (hSnapshot, &processEntry))
        return false;

    do {
        if (processEntry.th32ProcessID == ownPID) {
            CloseHandle (hSnapshot);

            *pPIDOut = processEntry.th32ParentProcessID;

            return true;
        }
    } while (Process32NextW (hSnapshot, &processEntry));

    return false;
}

bool WaitForProcess (WinHandle& hProcess)
{
    return WaitForSingleObject (hProcess.Get (), INFINITE) == WAIT_OBJECT_0;
}

} // namespace PTH