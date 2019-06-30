#include "Utils.hpp"

#include <tdh.h>
#include <wincrypt.h>

#include <memory>

#include "Utility/Asserts.hpp"
#include "Utility/OnExit.hpp"

namespace ETWP {

namespace {

std::vector<ProviderInfo> gCachedProviderInfos;

// /analyze believes that dereferencing pProviderInfos as nullptr is possible here. That would only happen if the first
//   call to TdhEnumerateProviders would produce an ERROR_SUCCESS status code, which is impossible (we pass nullptr for
//   the first parameter, making that call a mere size query)
#pragma warning(push)
#pragma warning(disable:6011)
bool GetRegisteredProvidersImpl (std::vector<ProviderInfo>* pProvidersOut)
{
    PROVIDER_ENUMERATION_INFO* pProviderInfos = nullptr;
    ULONG bufferSize = 0;
    DWORD status = TdhEnumerateProviders (nullptr, &bufferSize);
    std::unique_ptr<char[]> pRawData;
    // Race condition: the list of providers might change from query to query, so we need a while loop here...
    while (status == ERROR_INSUFFICIENT_BUFFER) {
        pRawData.reset (new char[bufferSize]);

        pProviderInfos = reinterpret_cast<PROVIDER_ENUMERATION_INFO*> (pRawData.get ());
        status = TdhEnumerateProviders (pProviderInfos, &bufferSize);
    }

    if (status != ERROR_SUCCESS)
        return false;

    for (DWORD i = 0; i < pProviderInfos->NumberOfProviders; ++i) {
        pProvidersOut->push_back ({ reinterpret_cast<WCHAR*> (reinterpret_cast<BYTE*> (pProviderInfos) +
                                        pProviderInfos->TraceProviderInfoArray[i].ProviderNameOffset),
                                    pProviderInfos->TraceProviderInfoArray[i].ProviderGuid });
    }

    return true;
}
#pragma warning(pop)

bool GetCachedRegisteredProviders (std::vector<ProviderInfo>* pProvidersOut)
{
    static bool cached = false;
    if (cached) {
        *pProvidersOut = gCachedProviderInfos;

        return true;
    }

    if (GetRegisteredProvidersImpl (&gCachedProviderInfos)) {
        cached = true;

        return GetCachedRegisteredProviders (pProvidersOut);
    } else {
        return false;
    }
}

}   // namespace

bool GetRegisteredProviders (std::vector<ProviderInfo>* pProvidersOut,
                             RegisteredProviderQueryPolicy queryPolicy /*= CachedResult*/)
{
    switch (queryPolicy) {
        case RegisteredProviderQueryPolicy::CachedResult:
            return GetCachedRegisteredProviders (pProvidersOut);

        case RegisteredProviderQueryPolicy::QueryNow:
            return GetRegisteredProvidersImpl (pProvidersOut);

        default:
            ETWP_DEBUG_BREAK_STR (L"Invalid RegisteredProviderQueryPolicy!");

            return false;
    }
}

bool IsProviderRegistered (const GUID& providerGUID,
                           RegisteredProviderQueryPolicy queryPolicy /*= CachedResult*/)
{
    std::vector<ProviderInfo> registeredProviders;
    if (!GetRegisteredProviders (&registeredProviders, queryPolicy))
        return false;

    for (auto&& provider : registeredProviders) {
        if (provider.guid == providerGUID)
            return true;
    }

    return false;
}

bool IsProviderRegistered (const std::wstring& providerName,
                           RegisteredProviderQueryPolicy queryPolicy /*= CachedResult*/)
{
    std::vector<ProviderInfo> registeredProviders;
    if (!GetRegisteredProviders (&registeredProviders, queryPolicy))
        return false;

    for (auto&& provider : registeredProviders) {
        if (_wcsicmp (provider.name.c_str (), providerName.c_str ()) == 0)
            return true;
    }

    return false;
}

bool GetGUIDOfRegisteredProvider (const std::wstring& providerName,
                                  GUID* pGUIDOut,
                                  RegisteredProviderQueryPolicy queryPolicy /*= CachedResult*/)
{
    std::vector<ProviderInfo> registeredProviders;
    if (!GetRegisteredProviders (&registeredProviders, queryPolicy))
        return false;

    for (auto&& provider : registeredProviders) {
        if (_wcsicmp (provider.name.c_str (), providerName.c_str ()) == 0) {
            *pGUIDOut = provider.guid;

            return true;
        }
    }

    return false;
}

bool GetNameOfRegisteredProvider (const GUID& providerGUID,
                                  std::wstring* pProviderNameOut,
                                  RegisteredProviderQueryPolicy queryPolicy /*= CachedResult*/)
{
    std::vector<ProviderInfo> registeredProviders;
    if (!GetRegisteredProviders (&registeredProviders, queryPolicy))
        return false;

    for (auto&& provider : registeredProviders) {
        if (providerGUID == provider.guid) {
            *pProviderNameOut = provider.name;

            return true;
        }
    }

    return false;
}

bool InferGUIDFromProviderName (const std::wstring& providerName, GUID* pGUIDOut)
{
    // This code is a direct port of Doug Cook's C# program found here: https://blogs.msdn.microsoft.com/dcook/2015/09/08/etw-provider-names-and-guids/

    static const unsigned char namespaceBytes[] = { 0x48, 0x2C, 0x2D, 0xB2, 0xC3, 0x90, 0x47, 0xC8, 0x87, 0xF8, 0x1A, 0x15, 0xBF, 0xC1, 0x30, 0xFB };

    // 1.) Make the input string UPPERCASE
    std::wstring providerNameCopy = providerName;
    for (auto&& c : providerNameCopy)
        c = towupper (c);

    // 2.a) Prepare for making the input data big endian by performing some calculations
    const size_t physLength = providerNameCopy.length () * sizeof (wchar_t) + sizeof L'\0';
    std::vector<unsigned char> pBENameBytes (physLength);
    const unsigned char* pProviderName = reinterpret_cast<const unsigned char*> (providerNameCopy.c_str ());

    // 2.b) (Poor man's) byte swap to get BE representation
    for (size_t i = 0; i < physLength - 1; i += 2) {
        pBENameBytes[i] = pProviderName[i + 1];
        pBENameBytes[i + 1] = pProviderName[i];
    }

    // 3.a) Win32 cryptography boilerplate
    HCRYPTPROV hProv;
    if (CryptAcquireContextW (&hProv, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT) != TRUE)
        return false;

    OnExit hProvCloser ([&hProv] () { CryptReleaseContext (hProv, 0); });

    HCRYPTHASH hHash;
    if (CryptCreateHash (hProv, CALG_SHA1, 0, 0, &hHash) != TRUE)
        return false;

    OnExit hHashCloser ([&hHash] () { CryptDestroyHash (hHash); });

    // 3.b) Hash the namespace bytes (predefined constant) with SHA1
    if (CryptHashData (hHash, namespaceBytes, sizeof namespaceBytes, 0) != TRUE)
        return false;

    // 3.c) Hash the uppercase, BE representation of the input string
    if (CryptHashData (hHash, &pBENameBytes[0], static_cast<DWORD> (physLength - 2), 0) != TRUE)
        return false;

    // 3.d) Acquire the hashed value after we query its size
    DWORD dataSize;
    DWORD sizeRead = sizeof dataSize;
    if (CryptGetHashParam (hHash, HP_HASHSIZE, (BYTE*) &dataSize, &sizeRead, 0) != TRUE)
        return false;

    std::vector<unsigned char> hashResult (dataSize);
    DWORD hashSizeRead = dataSize;
    if (CryptGetHashParam (hHash, HP_HASHVAL, &hashResult[0], &hashSizeRead, 0) != TRUE)
        return false;

    // 4.) Comment copied from Doug's code: "Guid = Hash[0..15], with Hash[7] tweaked according to RFC 4122"
    hashResult[7] = (hashResult[7] & 0x0F) | 0x50;

    // 5.) Bash the raw bytes into the output GUID structure
    memcpy (pGUIDOut, &hashResult[0], 16);

    return true;
}

}   // namespace ETWP