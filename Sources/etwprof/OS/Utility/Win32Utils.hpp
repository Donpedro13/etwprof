#ifndef ETWP_WIN32_UTILS_HPP
#define ETWP_WIN32_UTILS_HPP

#include <windows.h>

namespace ETWP {
namespace Win32 {

enum class WaitResult {
	Signaled,
	Timeout,
	Failed
};

WaitResult WaitForObject (HANDLE hObject, uint32_t timeout = INFINITE);

}   // namespace Win32
}   // namespace ETWP

#endif  // #ifndef ETWP_WIN32_UTILS_HPP