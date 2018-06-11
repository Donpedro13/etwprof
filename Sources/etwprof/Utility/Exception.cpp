#include "Exception.hpp"

namespace ETWP {

Exception::Exception (const std::wstring& msg):
    m_msg (msg)
{
}

const std::wstring& Exception::GetMsg () const
{
    return m_msg;
}

}   // namespace ETWP