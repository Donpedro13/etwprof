#ifndef ETWP_FS_UTILITY_HPP
#define ETWP_FS_UTILITY_HPP

#include <string>

namespace ETWP {

enum class Encoding {
    ACP,
    UTF8,
    UTF16LE,
    UTF16BE
};

std::wstring EncodingToString (Encoding encoding);

bool PathExists (const std::wstring& path);
// This function should work OK on UNC paths. In case of a network path
//   however, the path is considered invalid, if the host does not exist.
// Junctions and symlinks are untested/unsupported at this time.
bool PathValid (const std::wstring& path);

bool IsFile (const std::wstring& path);
bool IsDirectory (const std::wstring& path);

std::wstring PathGetDirectory (const std::wstring& path);
std::wstring PathGetFileName (const std::wstring& path);
std::wstring PathGetFileNameAndExtension (const std::wstring& path);
std::wstring PathGetExtension (const std::wstring& path);

std::wstring PathReplaceDirectory (const std::wstring& path, const std::wstring& newDirectory);
std::wstring PathReplaceFileName (const std::wstring& path, const std::wstring& newFilename);
std::wstring PathReplaceFileNameAndExtension (const std::wstring& path, const std::wstring& newFilenameAndExtension);
std::wstring PathReplaceExtension (const std::wstring& path, const std::wstring& newExtension);

std::wstring PathExpandEnvVars (const std::wstring& path);

bool FileDelete (const std::wstring& path);
bool FileRename (const std::wstring& oldPath, const std::wstring& newPath);
bool FileRenameByName (const std::wstring& oldPath, const std::wstring& newFileName);

}   // namespace ETWP

#endif  // #ifndef ETWP_FS_UTILITY_HPP