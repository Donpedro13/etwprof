#include "Utility.hpp"

#include <windows.h>

#include <filesystem>

namespace PTH {

std::wstring GetExePath ()
{
	WCHAR fullPathStr[MAX_PATH] = {};
	GetModuleFileNameW(nullptr, fullPathStr, MAX_PATH);

	return fullPathStr;
}

std::wstring GetExeFolderPath ()
{
	const std::wstring exePath = GetExePath ();

	return std::filesystem::path (exePath).remove_filename ();
}

} // namespace PTH