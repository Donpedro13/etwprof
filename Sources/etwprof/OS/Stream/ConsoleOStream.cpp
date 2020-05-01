#include "ConsoleOStream.hpp"

#include <memory>
#include <stdexcept>

#include "OStreamManipulators.hpp"
#include "Utility/Asserts.hpp"

namespace ETWP {

namespace {

std::unique_ptr<char> UTF16ToMultiByte (UINT codePage, const wchar_t* pString)
{
    int requiredBufferSize = WideCharToMultiByte (codePage,
                                                  0,
                                                  pString,
                                                  -1,
                                                  nullptr,
                                                  0, // No conversion; just get the buffer size
                                                  nullptr,
                                                  FALSE);

    std::unique_ptr<char> pBuffer (new char[requiredBufferSize]);

    WideCharToMultiByte (codePage,
                         0,
                         pString,
                         -1,
                         pBuffer.get (),
                         requiredBufferSize,
                         nullptr,
                         FALSE);

    return pBuffer;
}

const WORD ForegroundColorMap[] = {
    0,                                                                          /*Black*/
    FOREGROUND_INTENSITY | FOREGROUND_BLUE,                                     /*Blue*/
    FOREGROUND_BLUE,                                                            /*DarkBlue*/
    FOREGROUND_INTENSITY | FOREGROUND_GREEN | FOREGROUND_BLUE,                  /*Cyan*/
    FOREGROUND_GREEN | FOREGROUND_BLUE,                                         /*DarkCyan*/
    FOREGROUND_INTENSITY,                                                       /*Gray*/
    FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE,                        /*DarkGray*/
    FOREGROUND_INTENSITY | FOREGROUND_GREEN,                                    /*Green*/
    FOREGROUND_GREEN,                                                           /*DarkGreen*/
    FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_BLUE,                    /*Magenta*/
    FOREGROUND_RED | FOREGROUND_BLUE,                                           /*DarkMagenta*/
    FOREGROUND_INTENSITY | FOREGROUND_RED,                                      /*Red*/
    FOREGROUND_RED,                                                             /*DarkRed*/
    FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE, /*White*/
    FOREGROUND_INTENSITY | FOREGROUND_RED | FOREGROUND_GREEN,                   /*Yellow*/
    FOREGROUND_RED | FOREGROUND_GREEN                                           /*DarkYellow*/
};

const WORD BackgroundColorMap[] = {
    0,                                                                          /*Black*/
    BACKGROUND_INTENSITY | BACKGROUND_BLUE,                                     /*Blue*/
    BACKGROUND_BLUE,                                                            /*DarkBlue*/
    BACKGROUND_INTENSITY | BACKGROUND_GREEN | BACKGROUND_BLUE,                  /*Cyan*/
    BACKGROUND_GREEN | BACKGROUND_BLUE,                                         /*DarkCyan*/
    BACKGROUND_INTENSITY,                                                       /*Gray*/
    BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE,                        /*DarkGray*/
    BACKGROUND_INTENSITY | BACKGROUND_GREEN,                                    /*Green*/
    BACKGROUND_GREEN,                                                           /*DarkGreen*/
    BACKGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_BLUE,                    /*Magenta*/
    BACKGROUND_RED | BACKGROUND_BLUE,                                           /*DarkMagenta*/
    BACKGROUND_INTENSITY | BACKGROUND_RED,                                      /*Red*/
    BACKGROUND_RED,                                                             /*DarkRed*/
    BACKGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE, /*White*/
    BACKGROUND_INTENSITY | BACKGROUND_RED | BACKGROUND_GREEN,                   /*Yellow*/
    BACKGROUND_RED | BACKGROUND_GREEN                                           /*DarkYellow*/
};

constexpr WORD kForegroundBits = FOREGROUND_BLUE        |
                                 FOREGROUND_GREEN       |
                                 FOREGROUND_RED         |
                                 FOREGROUND_INTENSITY;

constexpr WORD kBackgroundBits = BACKGROUND_BLUE        |
                                 BACKGROUND_GREEN       |
                                 BACKGROUND_RED         |
                                 BACKGROUND_INTENSITY;

}   // namespace

ConsoleOStream::ConsoleOStream (StdHandle handle,
                                Encoding encoding/* = Encoding::Automatic*/):
    m_stdHandle (GetStdHandle (static_cast<DWORD> (handle))),
    m_encoding (encoding)
{
    if (ETWP_ERROR (handle != StdHandle::StdOut && handle != StdHandle::StdErr))
        throw std::runtime_error ("ConsoleOStream constructed with invalid StdHandle");

    if (ETWP_ERROR (m_stdHandle == INVALID_HANDLE_VALUE))
        throw std::runtime_error ("GetStdHandle returned INVALID_HANDLE_VALUE");

    InitType ();
    ResolveEncoding ();

    CONSOLE_SCREEN_BUFFER_INFO consoleBufferInfo;
    GetConsoleScreenBufferInfo (m_stdHandle, &consoleBufferInfo);

    m_origBgColor = consoleBufferInfo.wAttributes & kBackgroundBits;
    m_origFgColor = consoleBufferInfo.wAttributes & kForegroundBits;
}

ConsoleOStream::Type ConsoleOStream::GetType () const
{
    return m_type;
}

void ConsoleOStream::Write (const std::wstring& string)
{
    if (m_encoding == Encoding::UTF_16) {
        if (GetType () == Type::Console) {
            WriteUTF16ThroughConsole (string.c_str ());
        } else {
            WriteBytesThroughFile (string.c_str (),
                                   static_cast<DWORD> (string.length () * sizeof (wchar_t)));
        }
    } else if (m_encoding == Encoding::UTF_8) {
        // If someone writes to the console (and not a file or pipe)
        //   using UTF-8, that could only possibly work if the code page is
        //   UTF-8. That's so uncommon that it's worth an assert.
        ETWP_ASSERT (GetType () != Type::Console);

        std::unique_ptr<char> pBuffer = UTF16ToMultiByte (CP_UTF8, string.c_str ());

        if (GetType () == Type::Console)
            WriteANSIThroughConsole (pBuffer.get ());
        else
            WriteBytesThroughFile (pBuffer.get (), static_cast<DWORD> (strlen (pBuffer.get ())));
    } else if (m_encoding == Encoding::Environment) {
        if (GetType () == Type::Console) {
            std::unique_ptr<char> pBuffer = UTF16ToMultiByte (GetConsoleOutputCP (), string.c_str ());

            WriteANSIThroughConsole (pBuffer.get ());
        } else {
            std::unique_ptr<char> pBuffer = UTF16ToMultiByte (CP_ACP, string.c_str ());

            WriteBytesThroughFile (pBuffer.get (), static_cast<DWORD> (strlen (pBuffer.get ())));
        }
    }
}

void ConsoleOStream::WriteLine (const std::wstring& string)
{
    Write (string + EOL);
}

void ConsoleOStream::Write (const wchar_t* pString)
{
    Write (std::wstring (pString));
}

void ConsoleOStream::WriteLine (const wchar_t* pString)
{
    WriteLine (std::wstring (pString));
}

void ConsoleOStream::Write (wchar_t wChar)
{
    Write (std::wstring (1, wChar));
}

ConsoleOStream& ConsoleOStream::operator<< (const std::wstring& string)
{
    Write (string);

    return *this;
}

ConsoleOStream& ConsoleOStream::operator<< (const wchar_t* pString)
{
    Write (pString);

    return *this;
}

ConsoleOStream& ConsoleOStream::operator<< (wchar_t wChar)
{
    Write (wChar);

    return *this;
}

ConsoleOStream& ConsoleOStream::operator<< (OStreamManipulator manipulator)
{
    manipulator (this);

    return *this;
}

void ConsoleOStream::ResetColors ()
{
    if (m_type != Type::Console)
        return;

    ResetForegroundColor ();
    ResetBackgroundColor ();
}

void ConsoleOStream::SetForegroundColor (ConsoleColor newColor)
{
	if (m_type != Type::Console)
		return;

    ResetForegroundColor ();

    CONSOLE_SCREEN_BUFFER_INFO consoleBufferInfo;
    GetConsoleScreenBufferInfo (m_stdHandle, &consoleBufferInfo);

    WORD newAttributes = ClearForegroundColor(consoleBufferInfo.wAttributes);
    newAttributes |= ForegroundColorMap[static_cast<size_t> (newColor)];

    SetConsoleTextAttribute (m_stdHandle, newAttributes);
}

void ConsoleOStream::SetBackgroundColor (ConsoleColor newColor)
{
	if (m_type != Type::Console)
		return;

    CONSOLE_SCREEN_BUFFER_INFO consoleBufferInfo;
    GetConsoleScreenBufferInfo (m_stdHandle, &consoleBufferInfo);

    WORD newAttributes = ClearBackgroundColor(consoleBufferInfo.wAttributes);
    newAttributes |= BackgroundColorMap[static_cast<size_t> (newColor)];

    SetConsoleTextAttribute (m_stdHandle, newAttributes);
}

void ConsoleOStream::ResetForegroundColor ()
{
	if (m_type != Type::Console)
		return;

    CONSOLE_SCREEN_BUFFER_INFO consoleBufferInfo;
    GetConsoleScreenBufferInfo (m_stdHandle, &consoleBufferInfo);
    WORD newAttributes = ClearForegroundColor (consoleBufferInfo.wAttributes);
    newAttributes |= m_origFgColor;

    SetConsoleTextAttribute (m_stdHandle, newAttributes);
}

void ConsoleOStream::ResetBackgroundColor ()
{
	if (m_type != Type::Console)
		return;

    CONSOLE_SCREEN_BUFFER_INFO consoleBufferInfo;
    GetConsoleScreenBufferInfo (m_stdHandle, &consoleBufferInfo);
    WORD newAttributes = ClearBackgroundColor (consoleBufferInfo.wAttributes);
    newAttributes |= m_origBgColor;

    SetConsoleTextAttribute (m_stdHandle, newAttributes);
}

WORD ConsoleOStream::ClearForegroundColor (WORD attributes)
{
    return attributes & ~kForegroundBits;
}

WORD ConsoleOStream::ClearBackgroundColor (WORD attributes)
{
    return attributes & ~kBackgroundBits;
}

void ConsoleOStream::ResolveEncoding ()
{
    if (m_encoding != Encoding::Automatic)
        return;

    // In case of a console, we output UTF-16, UTF-8 otherwise (file, pipe, etc.)
    switch (GetType ()) {
        case Type::Console:
            m_encoding = Encoding::UTF_16;

            break;
        default:
			m_encoding = Encoding::UTF_8;

			break;
    }
}

void ConsoleOStream::InitType ()
{
	DWORD type = GetFileType (m_stdHandle);
	if (type == FILE_TYPE_DISK) {
		m_type = Type::File;
	}
	else if (type == FILE_TYPE_PIPE) {
        m_type = Type::Pipe;
	} else {
        // Note: FILE_TYPE_CHAR could be other types than just console, so we (ab)use GetConsoleMode instead...
		DWORD dummy;
        m_type = GetConsoleMode (m_stdHandle, &dummy) ? Type::Console : Type::Other;
	}
}

void ConsoleOStream::WriteBytesThroughFile (const void* pBytes, DWORD nBytes)
{
    ETWP_VERIFY (WriteFile (m_stdHandle, pBytes, nBytes, nullptr, nullptr));
}

void ConsoleOStream::WriteUTF16ThroughConsole (const wchar_t* pString)
{
    ETWP_VERIFY (WriteConsoleW (m_stdHandle,
                                pString,
                                static_cast<DWORD> (wcslen (pString)),
                                nullptr,
                                nullptr));
}

void ConsoleOStream::WriteANSIThroughConsole (const char* pString)
{
    ETWP_VERIFY (WriteConsoleA (m_stdHandle,
                                pString,
                                static_cast<DWORD> (strlen (pString)),
                                nullptr,
                                nullptr));
}

}   // namespace ETWP