#ifndef ETWP_STREAM_COMMON_HPP
#define ETWP_STREAM_COMMON_HPP

#include <windows.h>

namespace ETWP {

enum class StdHandle : DWORD {
    StdIn = STD_INPUT_HANDLE,
    StdOut = STD_OUTPUT_HANDLE,
    StdErr = STD_ERROR_HANDLE
};

enum class ConsoleColor : size_t {
    Black,
    Blue,
    DarkBlue,
    Cyan,
    DarkCyan,
    Gray,
    DarkGray,
    Green,
    DarkGreen,
    Magenta,
    DarkMagenta,
    Red,
    DarkRed,
    White,
    Yellow,
    DarkYellow
};

constexpr wchar_t EOL[] = L"\r\n";

}

#endif  // #ifndef ETWP_STREAM_COMMON_HPP