#ifndef ETWP_PROCESS_LIFETIME_EVENT_SOURCE_HPP
#define ETWP_PROCESS_LIFETIME_EVENT_SOURCE_HPP

#include <vector>

#include "OS/Utility/OSTypes.hpp"

namespace ETWP {

class ProcessLifetimeObserver;

class ProcessLifetimeEventSource {
public:
    virtual ~ProcessLifetimeEventSource ();

    void Attach (ProcessLifetimeObserver* pObserver);
    void Detach (ProcessLifetimeObserver* pObserver);

    void NotifyProcessStarted (PID pid, PID parentPID);
    void NotifyProcessEnded (PID pid, PID parentPID);

private:
    std::vector<ProcessLifetimeObserver*> m_pObservers;
};

}   // namespace ETWP

#endif  // #ifndef ETWP_PROCESS_LIFETIME_EVENT_SOURCE_HPP