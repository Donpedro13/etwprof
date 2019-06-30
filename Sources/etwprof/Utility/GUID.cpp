#include "GUID.hpp"

#include <windows.h>

#include <memory>

namespace ETWP {

bool IsGUID (const std::wstring& string)
{
    GUID dummy;

    return StringToGUID (string, &dummy);
}

bool StringToGUID (const std::wstring& string, GUID* pGUIDOut)
{
    constexpr size_t kGUIDStrMinLen = 36;   // Byte characters + four dashes
    if (string.length () < kGUIDStrMinLen)
        return false;

    std::wstring stringCopy = string;
    if (string[0] != L'{' && string.back () != L'}')  // IIDFromString requires leading '{' and trailing '}'
        stringCopy = L'{' + string + L'}';

    return IIDFromString (stringCopy.c_str (), pGUIDOut) == S_OK;
}

bool GUIDToString (const GUID& guid, std::wstring* pStringOut)
{
    constexpr size_t kGUIDLen = 128;    // Should be more than enough for a GUID
    std::unique_ptr<WCHAR[]> pResult (new WCHAR[kGUIDLen]);

    if (StringFromGUID2 (guid, pResult.get (), kGUIDLen) != 0) {
        *pStringOut = pResult.get ();

        return true;
    } else {
        return false;
    }
}

}   // namespace ETWP