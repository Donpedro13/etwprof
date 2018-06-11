#ifndef ETWP_ONEXIT_HPP
#define ETWP_ONEXIT_HPP

#include <functional>

#include "Utility/Macros.hpp"

namespace ETWP {

class OnExit final {
public:
    ETWP_DISABLE_COPY_AND_MOVE (OnExit);

    using ExitFunction = std::function<void ()>;

    OnExit (const ExitFunction& exitFunc);
    ~OnExit ();

    void Deactivate ();
    void Trigger ();

private:
    const ExitFunction m_exitFunc;
    bool               m_active;
};

}   // namespace ETWP

#endif  // #ifndef ETWP_ONEXIT_HPP