#ifndef ETWP_RESULT_HPP
#define ETWP_RESULT_HPP

#include <expected>
#include <string>

namespace ETWP {

// Contains an object of type R or an error string
template<typename R>
using Result = std::expected<R, std::wstring>;

using Error = std::unexpected<std::wstring>;

}   // namespace ETWP

#endif  // #ifndef ETWP_RESULT_HPP