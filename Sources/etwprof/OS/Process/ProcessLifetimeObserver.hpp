#ifndef ETWP_PROCESS_LIFETIME_OBSERVER_HPP
#define ETWP_PROCESS_LIFETIME_OBSERVER_HPP

#include <vector>

#include "OS/Utility/OSTypes.hpp"


namespace ETWP {

class ProcessLifetimeEventSource;

class ProcessLifetimeObserver {
public:
    virtual void ProcessStarted (PID pid, PID parentPID) = 0;
    virtual void ProcessEnded (PID pid, PID parentPID) = 0;

    virtual ~ProcessLifetimeObserver ();

private:
    friend ProcessLifetimeEventSource;  // Not nice, but better than use-after-free bugs

    std::vector<ProcessLifetimeEventSource*> m_eventSources;
};

}   // namespace ETWP

#endif  // #ifndef ETWP_PROCESS_LIFETIME_OBSERVER_HPP