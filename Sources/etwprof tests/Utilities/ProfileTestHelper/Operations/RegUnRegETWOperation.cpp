#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include "OperationRegistrar.hpp"
#include "Utility.hpp"

namespace PTH {
namespace {

bool ReplaceStringInFile (const std::wstring& filePath, const std::wstring& from, const std::wstring& to)
{
    // Read whole file into a string
    std::wifstream inFile (filePath);
    if (!inFile)
        return false;

    std::wstringstream contentStream;
    contentStream << inFile.rdbuf ();

    std::wstring content (contentStream.str());

    // Replace every occurrence
    size_t pos = 0;
    do {
        pos = content.find (from, pos);
        if (pos != std::wstring::npos) {
            content.replace(pos, from.length(), to);
            pos += to.length();
        }
    } while (pos != std::wstring::npos);

    // Write the updated content back to the original file
    inFile.close();
    std::wofstream outFile (filePath);
    if (!outFile)
        return false;

    outFile << content;

    return true;
}

bool RegUnregProviderImpl (const std::wstring& manifestPath, bool unreg)
{
    const std::wstring subCommand = unreg ? L"um" : L"im";
    const std::wstring command = L"wevtutil " + subCommand + L" \"" + manifestPath + L"\"";

    return _wsystem (command.c_str()) != 0;
}

bool RegisterProvider (const std::wstring& manifestPath)
{
    return RegUnregProviderImpl (manifestPath, false);
}

bool UnregisterProvider (const std::wstring& manifestPath)
{
    return RegUnregProviderImpl (manifestPath, true);
}

const std::wstring ManifestName = L"MB.man";
const std::wstring BinpathPlaceholder = L"{{BINPATH}}";

OperationRegistrator regRegistrator (L"RegETW", []() {
    const std::wstring exeFolderPath = GetExeFolderPath();
    if (exeFolderPath.empty())
        return false;

    // 1.) Replace BINPATH in manifest
    // 2.) Then try to unregister the provider (maybe it's still registered from a previous run)
    // 3.) Then finally register with winevtutil

    const std::wstring manifestPath = exeFolderPath + ManifestName;
    if (!ReplaceStringInFile (manifestPath, BinpathPlaceholder, GetExePath ()))
        return false;

    UnregisterProvider (manifestPath);

    if (!RegisterProvider (manifestPath))
        return false;

    return true;
});

OperationRegistrator unregRegistrator (L"UnregETW", []() {
    const std::wstring exeFolderPath = GetExeFolderPath();
    if (exeFolderPath.empty())
        return false;

    // 1.) Replace BINPATH in manifest
    // 2.) Unregister with winevtutil

    const std::wstring manifestPath = exeFolderPath + ManifestName;
    if (!ReplaceStringInFile (manifestPath, BinpathPlaceholder, GetExePath ()))
        return false;

    UnregisterProvider (manifestPath);

    return true;
});

}	// namespace
}	// namespace PTH