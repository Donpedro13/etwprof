#ifndef ETWP_ETW_UTILS_HPP
#define ETWP_ETW_UTILS_HPP

#include <windows.h>

#include <string>
#include <vector>

namespace ETWP {

struct ProviderInfo {
    std::wstring name;
    GUID guid;
};

// Querying all providers is slow (> 100 ms on my system), so by default, appropriate functions use a cached result
enum RegisteredProviderQueryPolicy {
    CachedResult,
    QueryNow
};

bool GetRegisteredProviders (std::vector<ProviderInfo>* pProvidersOut,
                             RegisteredProviderQueryPolicy queryPolicy = CachedResult);

bool IsProviderRegistered (const GUID& providerGUID,
                           RegisteredProviderQueryPolicy queryPolicy = CachedResult);
bool IsProviderRegistered (const std::wstring& providerName,
                           RegisteredProviderQueryPolicy queryPolicy = CachedResult);

bool GetGUIDOfRegisteredProvider (const std::wstring& providerName,
                                  GUID* pGUIDOut,
                                  RegisteredProviderQueryPolicy queryPolicy = CachedResult);
bool GetNameOfRegisteredProvider (const GUID& providerGUID,
                                  std::wstring* pProviderNameOut,
                                  RegisteredProviderQueryPolicy queryPolicy = CachedResult);

bool InferGUIDFromProviderName (const std::wstring& providerName, GUID* pGUIDOut);

}   // namespace ETWP

#endif  // #ifndef ETWP_ETW_UTILS_HPP