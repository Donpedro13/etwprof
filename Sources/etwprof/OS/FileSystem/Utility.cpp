#include "Utility.hpp"

#include <cstdlib>
#include <windows.h>

#include "Utility/Asserts.hpp"

namespace ETWP {

namespace {

constexpr DWORD kRegularFileFlags = FILE_ATTRIBUTE_ARCHIVE 	            |
                                    FILE_ATTRIBUTE_COMPRESSED           |
                                    FILE_ATTRIBUTE_ENCRYPTED            |
                                    FILE_ATTRIBUTE_HIDDEN               |
                                    FILE_ATTRIBUTE_NORMAL               |
                                    FILE_ATTRIBUTE_NOT_CONTENT_INDEXED  |
                                    FILE_ATTRIBUTE_OFFLINE              |
                                    FILE_ATTRIBUTE_READONLY	            |
                                    FILE_ATTRIBUTE_SPARSE_FILE          |
                                    FILE_ATTRIBUTE_SYSTEM               |
                                    FILE_ATTRIBUTE_TEMPORARY;

}   // namespace

bool PathExists (const std::wstring& path)
{
    // It would be more correct to call GetFileAttributesEx, check for failure,
    //   and decide based on GetLastError
    return GetFileAttributesW (path.c_str ()) != INVALID_FILE_ATTRIBUTES;
}

bool PathValid (const std::wstring& path)
{
    // System functions (GetFileAttributes, CreateFile, etc.) seem to return
    //   PATH_NOT_FOUND in case of an empty path, so we have to cherry-pick
    //   this case
    if (path.empty ())
        return false;

    SetLastError (0);
    GetFileAttributesW (path.c_str ());

    DWORD error = GetLastError ();

    // This is far from 100% perfect
    return error != ERROR_BAD_PATHNAME    &&
           error != ERROR_BAD_NETPATH     &&
           error != ERROR_BAD_DEVICE_PATH &&
           error != ERROR_INVALID_NAME;
}

bool IsFile (const std::wstring& path)
{
    DWORD attributes = GetFileAttributesW (path.c_str ());

    if (ETWP_ERROR (attributes == INVALID_FILE_ATTRIBUTES))
        return false;

    return (attributes & kRegularFileFlags) != 0;
}

bool IsDirectory (const std::wstring& path)
{
    DWORD attributes = GetFileAttributesW (path.c_str ());

    if (ETWP_ERROR (attributes == INVALID_FILE_ATTRIBUTES))
        return false;

    return (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
}

std::wstring PathGetDirectory (const std::wstring& path)
{
    wchar_t drive[_MAX_DRIVE] = L"";
    wchar_t dir[_MAX_DIR] = L"";

    _wsplitpath_s (path.c_str (),
                   drive,
                   _MAX_DRIVE,
                   dir,
                   _MAX_DIR,
                   nullptr,
                   0,
                   nullptr,
                   0);

    wchar_t result[_MAX_PATH] = L"";
    _wmakepath_s (result, drive, dir, nullptr, nullptr);

    return result;
}

std::wstring PathGetFileName (const std::wstring& path)
{
    wchar_t fileName[_MAX_FNAME] = L"";

    _wsplitpath_s (path.c_str (),
                   nullptr,
                   0,
                   nullptr,
                   0,
                   fileName,
                   _MAX_FNAME,
                   nullptr,
                   0);

    return fileName;
}

std::wstring PathGetFileNameAndExtension (const std::wstring& path)
{
    wchar_t fileName[_MAX_FNAME] = L"";
    wchar_t ext[_MAX_EXT] = L"";

    _wsplitpath_s (path.c_str (),
                   nullptr,
                   0,
                   nullptr,
                   0,
                   fileName,
                   _MAX_FNAME,
                   ext,
                   _MAX_EXT);

    wchar_t result[_MAX_PATH] = L"";
    _wmakepath_s (result, nullptr, nullptr, fileName, ext);

    return result;
}

std::wstring PathGetExtension (const std::wstring& path)
{
    wchar_t ext[_MAX_EXT] = L"";

    _wsplitpath_s (path.c_str (),
                   nullptr,
                   0,
                   nullptr,
                   0,
                   nullptr,
                   0,
                   ext,
                   _MAX_EXT);

    return ext;
}

std::wstring PathReplaceDirectory (const std::wstring& path,
                                   const std::wstring& newDirectory)
{
    WCHAR drive[_MAX_DRIVE];
    WCHAR filename[_MAX_FNAME];
    WCHAR extension[_MAX_EXT];

    _wsplitpath_s (path.c_str (),
                   drive,
                   _MAX_DRIVE,
                   nullptr,
                   0,
                   filename,
                   _MAX_FNAME,
                   extension,
                   _MAX_EXT);

    return drive + newDirectory + filename + extension;
}

std::wstring PathReplaceFileName (const std::wstring& path, const std::wstring& newFilename)
{
    WCHAR drive[_MAX_DRIVE];
    WCHAR folder[_MAX_DIR];
    WCHAR extension[_MAX_EXT];

    _wsplitpath_s (path.c_str (),
                   drive,
                   _MAX_DRIVE,
                   folder,
                   _MAX_DIR,
                   nullptr,
                   0,
                   extension,
                   _MAX_EXT);

    return drive + std::wstring (folder) + newFilename + extension;
}

std::wstring PathReplaceFileNameAndExtension (const std::wstring& path, const std::wstring& newFilenameAndExtension)
{
    WCHAR drive[_MAX_DRIVE];
    WCHAR folder[_MAX_DIR];

    _wsplitpath_s (path.c_str (),
                   drive,
                   _MAX_DRIVE,
                   folder,
                   _MAX_DIR,
                   nullptr,
                   0,
                   nullptr,
                   0);

    return drive + std::wstring (folder) + newFilenameAndExtension;
}

std::wstring PathReplaceExtension (const std::wstring& path, const std::wstring& newExtension)
{
    WCHAR drive[_MAX_DRIVE];
    WCHAR folder[_MAX_DIR];
    WCHAR filename[_MAX_FNAME];

    _wsplitpath_s (path.c_str (),
                   drive,
                   _MAX_DRIVE,
                   folder,
                   _MAX_DIR,
                   filename,
                   _MAX_FNAME,
                   nullptr,
                   0);

    return drive + std::wstring (folder) + filename + newExtension;
}

std::wstring PathExpandEnvVars (const std::wstring& path)
{
    if (path.length () > MAX_PATH - 1)  // -1 -> terminating zero
        return path;

    WCHAR expandedPath[MAX_PATH] = { '\0' };
    DWORD result = ExpandEnvironmentStringsW (path.c_str (), expandedPath, MAX_PATH);
    if (result > MAX_PATH || result == 0)  // Buffer too small or call failed
        return path;
    else
        return expandedPath;
}

bool FileDelete (const std::wstring& path)
{
    return DeleteFileW (path.c_str ()) != FALSE;
}

bool FileRename (const std::wstring& oldPath, const std::wstring& newPath)
{
    return MoveFileW (oldPath.c_str (), newPath.c_str ()) != FALSE;
}

bool FileRenameByName (const std::wstring& oldPath, const std::wstring& newFileName)
{
    WCHAR drive[_MAX_DRIVE];
    WCHAR folder[_MAX_DIR];
    WCHAR extension[_MAX_EXT];

    if (_wsplitpath_s (oldPath.c_str (),
                       drive,
                       _MAX_DRIVE,
                       folder,
                       _MAX_DIR,
                       nullptr,
                       0,
                       extension,
                       _MAX_EXT) != 0)
    {
        return false;
    }

    const std::wstring newPath = std::wstring (drive) + folder + newFileName + extension;
    return FileRename (oldPath, newPath);
}

}   // namespace ETWP