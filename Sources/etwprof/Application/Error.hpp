#ifndef ETWP_ERROR_HPP
#define ETWP_ERROR_HPP

#include <string>

namespace ETWP {

enum class GlobalErrorCodes {
    ErrorCodeMin = -1,   // Dummy; do not use

    InitializationError = 259 + 1,  // STILL_ACTIVE + 1 (not including windows.h to avoid "pollution")
    DeinitializationError,
    ProfilingInitializationError,

    ErrorCodeMax    // Dummy; do not use
};

extern const wchar_t* ErrorCodeDescriptions[];

}   // namespace ETWP

#endif  // #ifndef ETWP_ERROR_HPP