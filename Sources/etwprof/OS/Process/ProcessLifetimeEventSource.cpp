#include "ProcessLifetimeEventSource.hpp"

#include "ProcessLifetimeObserver.hpp"

#include "Utility/Asserts.hpp"

namespace ETWP {

ProcessLifetimeEventSource::~ProcessLifetimeEventSource ()
{
    for (const auto pObserver : m_pObservers) {
        Detach (pObserver);
    }
}

void ProcessLifetimeEventSource::Attach (ProcessLifetimeObserver* pObserver)
{
    ETWP_ASSERT (pObserver != nullptr);
    ETWP_ASSERT (std::find (m_pObservers.cbegin (), m_pObservers.cend (), pObserver) == m_pObservers.cend ());

    m_pObservers.push_back (pObserver);

    pObserver->m_eventSources.push_back (this);   // Not nice, but better than use-after-free bugs
}

void ProcessLifetimeEventSource::Detach (ProcessLifetimeObserver* pObserver)
{
    ETWP_ASSERT (pObserver != nullptr);
    ETWP_ASSERT (std::find (m_pObservers.cbegin (), m_pObservers.cend (), pObserver) != m_pObservers.end ());

    std::erase (pObserver->m_eventSources, this);   // Not nice, but better than use-after-free bugs
    std::erase (m_pObservers, pObserver);
}

void ProcessLifetimeEventSource::NotifyProcessStarted (PID pid, PID parentPID)
{
    for (const auto pObserver : m_pObservers)
        pObserver->ProcessStarted (pid, parentPID);
}

void ProcessLifetimeEventSource::NotifyProcessEnded (PID pid, PID parentPID)
{
    for (const auto pObserver : m_pObservers)
        pObserver->ProcessEnded (pid, parentPID);
}

}   // namespace ETWP