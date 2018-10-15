#include "Minidump.hpp"

#include <dbghelp.h>

#include "OS/FileSystem/Utility.hpp"

#include "Utility/Asserts.hpp"
#include "Utility/OnExit.hpp"

namespace ETWP {

namespace {

const HANDLE kDbgHelpHandle = reinterpret_cast<HANDLE> (0x1);

}   // namespace

 bool CreateMinidump (const std::wstring& dumpPath,
                      HANDLE hProcess,
                      DWORD processPID,
                      uint32_t dumpFlags,
                      std::wstring* pErrorOut)
{
    if (ETWP_ERROR (!PathValid (dumpPath))) {
        *pErrorOut = L"Path is invalid!";

        return false;
    }

	HANDLE hMinidump = CreateFileW (dumpPath.c_str (), GENERIC_WRITE, 0, nullptr, OPEN_ALWAYS, 0, 0);
    if (ETWP_ERROR (hMinidump == INVALID_HANDLE_VALUE)) {
        *pErrorOut = L"Unable to create minidump file (CreateFileW failed)!";

        return false;
    }

    OnExit handleCloser ([&hMinidump]() { CloseHandle (hMinidump); });

    SymInitializeW (kDbgHelpHandle, L"", FALSE);
    OnExit dbgHelpUninitializer ([]() { SymCleanup (kDbgHelpHandle); });
	
	BOOL result = MiniDumpWriteDump (hProcess,
									 processPID,
                                     hMinidump,
									 static_cast<MINIDUMP_TYPE> (dumpFlags),
									 nullptr,
									 nullptr,
									 nullptr);

    if (ETWP_ERROR (result == FALSE)) {
        *pErrorOut = L"Unable to write minidump (MiniDumpWriteDump failed)!";

        return false;
    } else {
        return true;
    }
}

}   // namespace ETWP