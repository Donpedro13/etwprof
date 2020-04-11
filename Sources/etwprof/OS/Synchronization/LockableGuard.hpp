#ifndef ETWP_LOCKABLE_GUARD_HPP
#define ETWP_LOCKABLE_GUARD_HPP

#include "Utility/Macros.hpp"

namespace ETWP {

template<typename T>
concept Lockable = requires (T t) { t.Lock (); t.Unlock (); };

template <class T> requires Lockable<T>
class LockableGuard final {
public:
    ETWP_DISABLE_COPY_AND_MOVE (LockableGuard);

    LockableGuard (T* lockable):
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
    T*   m_lockable;
    bool m_shouldUnlock;
};

}   // namespace ETWP

#endif  // #ifndef ETWP_LOCKABLE_GUARD_HPP