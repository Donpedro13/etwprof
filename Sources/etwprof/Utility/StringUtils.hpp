#ifndef ETWP_STRINGUTILS_HPP
#define ETWP_STRINGUTILS_HPP

#include <string>
#include <vector>

namespace ETWP {

std::vector<std::wstring> SplitString (const std::wstring& string, wchar_t delimiter);

}   // namespace ETWP

#endif  // #ifndef ETWP_STRINGUTILS_HPP