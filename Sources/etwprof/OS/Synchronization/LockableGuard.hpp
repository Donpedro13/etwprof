#ifndef ETWP_LOCKABLE_GUARD_HPP
#define ETWP_LOCKABLE_GUARD_HPP

#include "Utility/Macros.hpp"

namespace ETWP {

// Lockable: a class that has these two methods:
//   - void Lock ()
//   - void Unlock ()
template <class Lockable>
class LockableGuard final {
public:
    ETWP_DISABLE_COPY_AND_MOVE (LockableGuard);

    LockableGuard (Lockable* lockable):
        m_lockable (lockable),
        m_shouldUnlock (true)
    {
        m_lockable->Lock ();
    }

    ~LockableGuard ()
    {
        if (m_shouldUnlock)
            m_lockable->Unlock ();
    }

    void Release ()
    {
        m_lockable->Unlock ();
        m_shouldUnlock = false;
    }

private:
    Lockable* m_lockable;
    bool      m_shouldUnlock;
};

}   // namespace ETWP

#endif  // #ifndef ETWP_LOCKABLE_GUARD_HPP