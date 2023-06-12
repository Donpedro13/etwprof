#include "Event.hpp"

#include "Utility/Asserts.hpp"

namespace ETWP {

namespace {
bool WaitImpl (HANDLE handle, Timeout timeout)
{
	switch (WaitForSingleObject (handle, timeout)) {
        case WAIT_OBJECT_0:
            return true;

        case WAIT_TIMEOUT:
            return false;

        case WAIT_FAILED:
            [[fallthrough]];
        default:
            ETWP_DEBUG_BREAK_STR (L"Impossible value returned from WaitForSingleObject in " __FUNCTIONW__  L"!");

            return false;
    }
}
}   // namespace

Event::InitException::InitException (const std::wstring& msg): Exception (msg)
{
}

Event::Event (): m_hEvent (CreateEventW (nullptr, TRUE, FALSE, nullptr))
{
	if (m_hEvent == nullptr)
		throw InitException (L"Unable to create event (CreateEventW failed)!");
}

Event::~Event ()
{
	CloseHandle (m_hEvent);
}

void Event::Set ()
{
	ETWP_VERIFY (SetEvent (m_hEvent));
}

void Event::Reset ()
{
	ETWP_VERIFY (ResetEvent (m_hEvent));
}

bool Event::IsSet ()
{
    return WaitImpl (m_hEvent, 0);
}

bool Event::Wait (Timeout timeout/* = Infinite*/)
{
    return WaitImpl (m_hEvent, timeout);
}

}   // namespace ETWP