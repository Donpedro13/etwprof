#include "IETWBasedProfiler.hpp"

namespace ETWP {

bool operator== (const IETWBasedProfiler::ProviderInfo& lhs, const IETWBasedProfiler::ProviderInfo& rhs)
{
    return lhs.providerID == rhs.providerID;
}

}   // namespace ETWP