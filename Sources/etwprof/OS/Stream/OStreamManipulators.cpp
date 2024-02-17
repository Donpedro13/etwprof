#include "OStreamManipulators.hpp"

#include "ConsoleOStream.hpp"

namespace ETWP {

void Clearl (ConsoleOStream* stream)
{
    stream->ClearLine ();
}

void Endl (ConsoleOStream* stream)
{
    stream->Write (EOL);
}

void ColorReset (ConsoleOStream* stream)
{
    stream->ResetColors ();
}

void FgColorReset (ConsoleOStream* stream)
{
    stream->ResetForegroundColor ();
}

void FgColorBlack (ConsoleOStream* stream)
{
    stream->SetForegroundColor (ConsoleColor::Black);
}

void FgColorBlue (ConsoleOStream* stream)
{
    stream->SetForegroundColor (ConsoleColor::Blue);
}

void FgColorDarkBlue (ConsoleOStream* stream)
{
    stream->SetForegroundColor (ConsoleColor::DarkBlue);
}

void FgColorCyan (ConsoleOStream* stream)
{
    stream->SetForegroundColor (ConsoleColor::Cyan);
}

void FgColorDarkCyan (ConsoleOStream* stream)
{
    stream->SetForegroundColor (ConsoleColor::DarkCyan);
}

void FgColorGray (ConsoleOStream* stream)
{
    stream->SetForegroundColor (ConsoleColor::Gray);
}

void FgColorDarkGray (ConsoleOStream* stream)
{
    stream->SetForegroundColor (ConsoleColor::DarkGray);
}

void FgColorGreen (ConsoleOStream* stream)
{
    stream->SetForegroundColor (ConsoleColor::Green);
}

void FgColorDarkGreen (ConsoleOStream* stream)
{
    stream->SetForegroundColor (ConsoleColor::DarkGreen);
}

void FgColorMagenta (ConsoleOStream* stream)
{
    stream->SetForegroundColor (ConsoleColor::Magenta);
}

void FgColorDarkMagenta (ConsoleOStream* stream)
{
    stream->SetForegroundColor (ConsoleColor::DarkMagenta);
}

void FgColorRed (ConsoleOStream* stream)
{
    stream->SetForegroundColor (ConsoleColor::Red);
}

void FgColorDarkRed (ConsoleOStream* stream)
{
    stream->SetForegroundColor (ConsoleColor::DarkRed);
}

void FgColorWhite (ConsoleOStream* stream)
{
    stream->SetForegroundColor (ConsoleColor::White);
}

void FgColorYellow (ConsoleOStream* stream)
{
    stream->SetForegroundColor (ConsoleColor::Yellow);
}

void FgColorDarkYellow (ConsoleOStream* stream)
{
    stream->SetForegroundColor (ConsoleColor::DarkYellow);
}

void BgColorReset (ConsoleOStream* stream)
{
    stream->ResetBackgroundColor ();
}

void BgColorBlack (ConsoleOStream* stream)
{
    stream->SetBackgroundColor (ConsoleColor::Black);
}

void BgColorBlue (ConsoleOStream* stream)
{
    stream->SetBackgroundColor (ConsoleColor::Blue);
}

void BgColorDarkBlue (ConsoleOStream* stream)
{
    stream->SetBackgroundColor (ConsoleColor::DarkBlue);
}

void BgColorCyan (ConsoleOStream* stream)
{
    stream->SetBackgroundColor (ConsoleColor::Cyan);
}

void BgColorDarkCyan (ConsoleOStream* stream)
{
    stream->SetBackgroundColor (ConsoleColor::DarkCyan);
}

void BgColorGray (ConsoleOStream* stream)
{
    stream->SetBackgroundColor (ConsoleColor::Gray);
}

void BgColorDarkGray (ConsoleOStream* stream)
{
    stream->SetBackgroundColor (ConsoleColor::DarkGray);
}

void BgColorGreen (ConsoleOStream* stream)
{
    stream->SetBackgroundColor (ConsoleColor::Green);
}

void BgColorDarkGreen (ConsoleOStream* stream)
{
    stream->SetBackgroundColor (ConsoleColor::DarkGreen);
}

void BgColorMagenta (ConsoleOStream* stream)
{
    stream->SetBackgroundColor (ConsoleColor::Magenta);
}

void BgColorDarkMagenta (ConsoleOStream* stream)
{
    stream->SetBackgroundColor (ConsoleColor::DarkMagenta);
}

void BgColorRed (ConsoleOStream* stream)
{
    stream->SetBackgroundColor (ConsoleColor::Red);
}

void BgColorDarkRed (ConsoleOStream* stream)
{
    stream->SetBackgroundColor (ConsoleColor::DarkRed);
}

void BgColorWhite (ConsoleOStream* stream)
{
    stream->SetBackgroundColor (ConsoleColor::White);
}

void BgColorYellow (ConsoleOStream* stream)
{
    stream->SetBackgroundColor (ConsoleColor::Yellow);
}

void BgColorDarkYellow (ConsoleOStream* stream)
{
    stream->SetBackgroundColor (ConsoleColor::DarkYellow);
}

}   // namespace ETWP