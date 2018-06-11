#ifndef ETWP_CONSOLE_OSTREAM_HPP
#define ETWP_CONSOLE_OSTREAM_HPP    

#include <memory>
#include <string>

#include "StreamCommon.hpp"
#include "OStreamManipulators.hpp"

#include "Utility/Macros.hpp"

namespace ETWP {

// Unbuffered stream that outputs to a "console-like" output
// If this class turns out to be a bottleneck, there is plenty of room
//   for optimizations, and/or we could introduce buffering.
class ConsoleOStream final {
public:
    enum class Encoding {
        Automatic,
        UTF_8,
        UTF_16,
        Environment  // Console's CP or CP_ACP
    };

    explicit ConsoleOStream (StdHandle handle,
                             Encoding encoding = Encoding::Automatic);

    // Disabled operations
    ConsoleOStream () = delete;
    ETWP_DISABLE_COPY_AND_MOVE (ConsoleOStream);

    bool IsOfConsoleType () const;

    void Write (const std::wstring& string);
    void WriteLine (const std::wstring& string);
    void Write (const wchar_t* pString);
    void WriteLine (const wchar_t* pString);
    void Write (wchar_t wChar);

    ConsoleOStream& operator<< (const std::wstring& string);
    ConsoleOStream& operator<< (const wchar_t* pString);
    ConsoleOStream& operator<< (wchar_t wChar);
    ConsoleOStream& operator<< (OStreamManipulator manipulator);

    void SetForegroundColor (ConsoleColor newColor);
    void SetBackgroundColor (ConsoleColor newColor);
    void ResetColors ();
    void ResetForegroundColor ();
    void ResetBackgroundColor ();

private:
    HANDLE   m_stdHandle = INVALID_HANDLE_VALUE;
    Encoding m_encoding;
    WORD     m_origBgColor;
    WORD     m_origFgColor;

    WORD ClearForegroundColor (WORD attributes);
    WORD ClearBackgroundColor (WORD attributes);


    void ResolveEncoding ();

    // These functions are helpers; they are not meant to convert
    void WriteBytesThroughFile (const void* pBytes, DWORD nBytes);
    void WriteUTF16ThroughConsole (const wchar_t* pString);
    void WriteANSIThroughConsole (const char* pString);
};

}   // namespace ETWP

#endif  // #ifndef ETWP_CONSOLE_OSTREAM_HPP