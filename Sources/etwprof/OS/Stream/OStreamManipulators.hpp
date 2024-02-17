#ifndef ETWP_OSTREAM_MANIPULATORS_HPP
#define ETWP_OSTREAM_MANIPULATORS_HPP

namespace ETWP {

class ConsoleOStream;

using OStreamManipulator = void (*)(ConsoleOStream*);

void Clearl (ConsoleOStream* stream);
void Endl (ConsoleOStream* stream);

void ColorReset (ConsoleOStream* stream);

void FgColorReset (ConsoleOStream* stream);
void FgColorBlack (ConsoleOStream* stream);
void FgColorBlue (ConsoleOStream* stream);
void FgColorDarkBlue (ConsoleOStream* stream);
void FgColorCyan (ConsoleOStream* stream);
void FgColorDarkCyan (ConsoleOStream* stream);
void FgColorGray (ConsoleOStream* stream);
void FgColorDarkGray (ConsoleOStream* stream);
void FgColorGreen (ConsoleOStream* stream);
void FgColorDarkGreen (ConsoleOStream* stream);
void FgColorMagenta (ConsoleOStream* stream);
void FgColorDarkMagenta (ConsoleOStream* stream);
void FgColorRed (ConsoleOStream* stream);
void FgColorDarkRed (ConsoleOStream* stream);
void FgColorWhite (ConsoleOStream* stream);
void FgColorYellow (ConsoleOStream* stream);
void FgColorDarkYellow (ConsoleOStream* stream);

void BgColorReset (ConsoleOStream* stream);
void BgColorBlack (ConsoleOStream* stream);
void BgColorBlue (ConsoleOStream* stream);
void BgColorDarkBlue (ConsoleOStream* stream);
void BgColorCyan (ConsoleOStream* stream);
void BgColorDarkCyan (ConsoleOStream* stream);
void BgColorGray (ConsoleOStream* stream);
void BgColorDarkGray (ConsoleOStream* stream);
void BgColorGreen (ConsoleOStream* stream);
void BgColorDarkGreen (ConsoleOStream* stream);
void BgColorMagenta (ConsoleOStream* stream);
void BgColorDarkMagenta (ConsoleOStream* stream);
void BgColorRed (ConsoleOStream* stream);
void BgColorDarkRed (ConsoleOStream* stream);
void BgColorWhite (ConsoleOStream* stream);
void BgColorYellow (ConsoleOStream* stream);
void BgColorDarkYellow (ConsoleOStream* stream);

}   // namespace ETWP

#endif  // #ifndef ETWP_OSTREAM_MANIPULATORS_HPP