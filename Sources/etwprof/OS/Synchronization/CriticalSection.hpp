#ifndef ETWP_CRITICAL_SECTION_HPP
#define ETWP_CRITICAL_SECTION_HPP

#include <windows.h>

#include "Utility/Macros.hpp"

namespace ETWP {

class CriticalSection final {
public:
    ETWP_DISABLE_COPY_AND_MOVE (CriticalSection);

    explicit CriticalSection (DWORD spinCount = 0);
    ~CriticalSection ();

    void Lock ();
    bool TryLock ();
    void Unlock ();

private:
    CRITICAL_SECTION m_cs;
};

}   // namespace ETWP

#endif  // #ifndef ETWP_CRITICAL_SECTION_HPP