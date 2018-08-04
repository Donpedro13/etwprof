#ifndef ETWP_ASSERTS_HPP
#define ETWP_ASSERTS_HPP

#include "Macros.hpp"

#ifdef ETWP_RELEASE
    #define ETWP_DEBUG_PRINT(str)
    #define ETWP_DEBUG_PRINT_LN(str)
    #define ETWP_DEBUG_BREAK()
    #define ETWP_DEBUG_BREAK_STR(str)
    #define ETWP_DEBUG_ONLY(stmt)
    #define ETWP_ASSERT(expr)
    #define ETWP_ERROR(expr) expr
    #define ETWP_VERIFY(expr) expr
#else
    #define ETWP_DEBUG_PRINT(str) Impl::DebugPrint (str);
    #define ETWP_DEBUG_PRINT_LN(str) Impl::DebugPrintLine (str);
    #define ETWP_DEBUG_BREAK() Impl::ExplicitBreakIn (ETWP_WFILE, __LINE__);
    #define ETWP_DEBUG_BREAK_STR(str) ETWP_DEBUG_PRINT(str) ETWP_DEBUG_BREAK()
    #define ETWP_DEBUG_ONLY(stmt) stmt
    #define ETWP_ASSERT(expr)                                                  \
        if (!(expr))                                                           \
            Impl::Assertion (L#expr, false, ETWP_WFILE, __LINE__)
    #define ETWP_ERROR(expr) ((expr) ?                                         \
                Impl::Assertion (L#expr, true, ETWP_WFILE, __LINE__) : false)
    #define ETWP_VERIFY(expr) (!(expr) ?                                       \
                Impl::Assertion (L#expr, false, ETWP_WFILE, __LINE__) : true)
#endif  // #ifdef ETWP_RELEASE

namespace ETWP {

namespace Impl {

bool ExplicitBreakIn (const wchar_t* file, int line);
bool Assertion (const wchar_t* expr, bool value, const wchar_t* file, int line);
void DebugPrint (const wchar_t* str);
void DebugPrintLine (const wchar_t* str);

}   // namespace Impl

}   // namespace ETWP

#endif  // #ifndef ETWP_ASSERTS_HPP