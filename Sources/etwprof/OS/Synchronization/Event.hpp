#ifndef ETWP_EVENT_HPP
#define ETWP_EVENT_HPP

#include <windows.h>

#include "OS/Utility/Time.hpp"

#include "Utility/Macros.hpp"
#include "Utility/Exception.hpp"

namespace ETWP {

class Event final {
public:
    ETWP_DISABLE_COPY_AND_MOVE (Event);

    class InitException : public Exception {
    public:
        InitException (const std::wstring& msg);
        virtual ~InitException() = default;
    };

    Event ();
    ~Event ();

    void Set ();
    void Reset ();

    bool IsSet ();

    bool Wait (Timeout timeout = Infinite);

private:
    HANDLE m_hEvent;
};

}   // namespace ETWP

#endif  // #ifndef ETWP_EVENT_HPP