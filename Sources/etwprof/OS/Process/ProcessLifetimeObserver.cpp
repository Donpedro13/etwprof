#include "ProcessLifetimeObserver.hpp"

#include "ProcessLifetimeEventSource.hpp"

namespace ETWP {

ProcessLifetimeObserver::~ProcessLifetimeObserver ()
{
	for (const auto pEventSource : m_eventSources)
		pEventSource->Detach (this);
}

}   // namespace ETWP