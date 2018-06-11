#ifndef ETWP_EXCEPTION_HPP
#define ETWP_EXCEPTION_HPP

#include <string>

namespace ETWP {

class Exception {
public:
    Exception (const std::wstring& msg);
    virtual ~Exception () = default;

    const std::wstring& GetMsg () const;

private:
    const std::wstring m_msg;
};

}   // namespace ETWP

#endif  // #ifndef ETWP_EXCEPTION_HPP