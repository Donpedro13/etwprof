#include "Win32Utils.hpp"

#include "Utility/Asserts.hpp"

namespace ETWP {
namespace Win32 {

WaitResult WaitForObject (HANDLE hObject, uint32_t timeout /*= INFINITE*/)
{
    switch (WaitForSingleObject (hObject, timeout)) {
        case WAIT_OBJECT_0:
            return WaitResult::Signaled;

        case WAIT_TIMEOUT:
            return WaitResult::Timeout;

        case WAIT_FAILED:
            return WaitResult::Failed;

        default:
            ETWP_DEBUG_BREAK_STR (L"Impossible value returned from WaitForSingleObject in " __FUNCTIONW__  L"!");

            std::abort ();
    }
}

}   // namespace Win32
}   // namespace ETWP