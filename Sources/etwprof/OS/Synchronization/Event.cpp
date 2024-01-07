#include "Event.hpp"

#include "OS/Utility/Win32Utils.hpp"

#include "Utility/Asserts.hpp"

namespace ETWP {

namespace {
bool WaitImpl (HANDLE handle, Timeout timeout)
{
    Win32::WaitResult result = Win32::WaitForObject (handle, timeout);
    
    ETWP_ASSERT (result == Win32::WaitResult::Signaled || result == Win32::WaitResult::Timeout);
    
    return result == Win32::WaitResult::Signaled;
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