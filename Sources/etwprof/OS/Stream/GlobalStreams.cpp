#include "GlobalStreams.hpp"

#include "ConsoleOStream.hpp"

namespace ETWP {

ConsoleOStream& COut ()
{
    static ConsoleOStream gCOut (StdHandle::StdOut);

    return gCOut;
}

ConsoleOStream& CErr ()
{
    static ConsoleOStream gCErr (StdHandle::StdErr);

    return gCErr;
}

}   // namespace ETWP