#ifndef ETWP_GUID_HPP
#define ETWP_GUID_HPP

#include <string>

struct _GUID;
typedef struct _GUID GUID;

namespace ETWP {

bool IsGUID (const std::wstring& string);

bool StringToGUID (const std::wstring& string, GUID* pGUIDOut);
bool GUIDToString (const GUID& guid, std::wstring* pStringOut);

}   // namespace ETWP

#endif  // #ifndef ETWP_GUID_HPP