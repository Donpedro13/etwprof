#ifndef ETWP_I_ETW_BASED_PROFILER_HPP
#define ETWP_I_ETW_BASED_PROFILER_HPP

#include <windows.h>

#include "IProfiler.hpp"

#include "Utility/Macros.hpp"

namespace ETWP {

// Interface for thread-safe ETW-based profiler objects
class IETWBasedProfiler : public IProfiler {
public:
    ETWP_DISABLE_COPY_AND_MOVE (IETWBasedProfiler);

    struct ProviderInfo {
        GUID      providerID;
        bool      stack;
        UCHAR     level;
        ULONGLONG flags;
    };

    using Flags = uint8_t;

    enum Options : Flags {
        Default         = 0b0000,     
        RecordCSwitches = 0b0001,     // Record context switch information
        Compress        = 0b0010,     // Compress result ETL with ETW's built-in compression
        Debug           = 0b0100,     // Preserve intermediate ETL files (if any)
        StackCache      = 0b1000      // Use ETW's stack caching feature
    };

    IETWBasedProfiler () = default;
    virtual ~IETWBasedProfiler () = default;

    virtual bool EnableProvider (const ProviderInfo& providerInfo) = 0;
};

bool operator== (const IETWBasedProfiler::ProviderInfo& lhs, const IETWBasedProfiler::ProviderInfo& rhs);

}   // namespace ETWP

#endif  // #ifndef ETWP_I_ETW_BASED_PROFILER_HPP