#ifndef PTH_UTILITY_HPP
#define PTH_UTILITY_HPP

#include <windows.h>

#include <string>

namespace PTH {

class WinHandle {
public:
    WinHandle (HANDLE handle);
    WinHandle (WinHandle&& rhs);
    ~WinHandle ();

    HANDLE Get ();

private:
    HANDLE m_handle;
};

[[noreturn]] void Fail (const std::wstring& msg);

std::wstring GetExePath ();
std::wstring GetExeFolderPath ();

DWORD GetParentPID ();

}   // namespace PTH

#endif  // #ifndef PTH_UTILITY_HPP