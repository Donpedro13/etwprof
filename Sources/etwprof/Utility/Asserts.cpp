#include "Asserts.hpp"

#include <windows.h>

#include <string>

namespace ETWP {

namespace {

const std::wstring kAssertTitle = L"Assertion failed!";

void Break ()
{
    DebugBreak ();
}


bool AssertionImpl (const std::wstring& title, const std::wstring& message)
{
    Impl::DebugPrintLine (title.c_str ());
    Impl::DebugPrintLine (message.c_str ());

    const std::wstring fullMessage = message + L"\n\nDo you want to break in?";

    int result = MessageBoxW (nullptr, fullMessage.c_str (), title.c_str (), MB_ICONWARNING | MB_YESNO);

    if (result == IDYES)
        Break ();

    return true;
}

}   // namespace

namespace Impl {

bool ExplicitBreakIn (const wchar_t* file, int line)
{
    std::wstring message = L"At " + std::wstring (file) + L":" + std::to_wstring (line) +
        L"\nExplicit break-in was requested!";

    return AssertionImpl (kAssertTitle, message);
}

bool Assertion (const wchar_t* expr, bool value, const wchar_t* file, int line)
{
    const std::wstring title = L"Assertion failed!";
    std::wstring locMessage = L"At " + std::wstring (file) + L":" + std::to_wstring (line);

    std::wstring fullMessage = locMessage + L'\n' + expr + L" was " + (value ? L"true" : L"false") + L"!";

    return AssertionImpl (title, fullMessage);
}

void DebugPrint (const wchar_t* str)
{
    OutputDebugStringW (str);
}

void DebugPrintLine (const wchar_t* str)
{
    DebugPrint (str);
    DebugPrint (L"\n");
}

}   // namespace Impl

}   // namespace ETWP