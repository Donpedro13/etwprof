#ifndef ETWP_RESPONSE_FILE_HPP
#define ETWP_RESPONSE_FILE_HPP

#include <memory>
#include <string>
#include <vector>

#include "OS/FileSystem/Utility.hpp"

#include "Utility/Exception.hpp"
#include "Utility/Macros.hpp"

namespace ETWP {

namespace Impl { class ResponseFileImpl; }

class ResponseFile final {
public:
    class InitException : public Exception {
    public:
        InitException (const std::wstring& msg);
    };

    ETWP_DISABLE_COPY_AND_MOVE (ResponseFile);

    explicit ResponseFile (const std::wstring& filePath);
    ~ResponseFile ();

    bool Unpack (std::vector<std::wstring>* pArgumentsOut, std::wstring* pErrorOut);

    Encoding GetEncoding () const;

private:
    std::unique_ptr<Impl::ResponseFileImpl> m_impl;
};

}   // namespace ETWP

#endif  // #ifndef ETWP_RESPONSE_FILE_HPP