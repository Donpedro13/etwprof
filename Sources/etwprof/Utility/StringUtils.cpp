#include "StringUtils.hpp"

#include <algorithm>
#include <utility>

namespace ETWP {

std::vector<std::wstring> SplitString (const std::wstring& string, wchar_t delimiter)
{
    std::vector<std::wstring> result;
    std::wstring remaining = string;
    std::wstring::size_type delimiterI;

    do {
        delimiterI = remaining.find (delimiter);
        result.push_back (remaining.substr (0, delimiterI));
        remaining = remaining.substr (delimiterI + 1, std::wstring::npos);
    } while (delimiterI != std::wstring::npos);

    return result;
}

}   // namespace ETWP