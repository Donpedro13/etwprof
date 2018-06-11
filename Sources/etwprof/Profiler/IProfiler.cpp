#include "IProfiler.hpp"

namespace ETWP {

IProfiler::InitException::InitException (const std::wstring& msg):
    Exception (msg)
{
}

}   // namespace ETWP