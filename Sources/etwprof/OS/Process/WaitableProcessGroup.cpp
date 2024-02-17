#include "WaitableProcessGroup.hpp"

#include <chrono>
using namespace std::chrono_literals;

#include "Utility/Asserts.hpp"
#include "Utility/OnExit.hpp"

// Currently, /analyze is not a friend of std::timed_mutex and others (I guess this is the same as:
//   https://developercommunity.visualstudio.com/t/lock_guard-with-shared_timed_mutex-SCA-a/10370951)
#pragma warning (disable: 26110)

namespace ETWP {
namespace {

struct WaitCallbackContext {
    WaitableProcessGroup* pParent;
    const ProcessRef* pProcess;
    bool canceled;
};

}   // namespace

WaitableProcessGroup::ConstIterator::ConstIterator (const base_iterator& origIt,
                                                    const WaitableProcessGroup* pParent):
    m_pParent (pParent),
    m_baseIt (origIt)
{
    m_pParent->m_lock.lock ();
}

WaitableProcessGroup::ConstIterator::ConstIterator (const ConstIterator& rhs):
    ConstIterator (rhs.m_baseIt, rhs.m_pParent)
{
}

WaitableProcessGroup::ConstIterator::ConstIterator (const ConstIterator&& rhs) noexcept :
    ConstIterator (rhs.m_baseIt, rhs.m_pParent) // Cheeky, ugly, but works
{
}

WaitableProcessGroup::ConstIterator::~ConstIterator ()
{
    m_pParent->m_lock.unlock ();
}

WaitableProcessGroup::WaitableProcessGroup (std::vector<ProcessRef>&& processes)
{
    for (auto&& process : processes) {
        ETWP_ASSERT (process.GetOptions () & ProcessRef::Synchronize);

        Add (std::move (process));
    }
}

WaitableProcessGroup::~WaitableProcessGroup ()
{
    std::lock_guard guard (m_lock);

    for (auto& [_, waitContext] : m_waitContexts)
        CancelWait (&waitContext, true);
}

void WaitableProcessGroup::Add (ProcessRef&& process)
{
    std::lock_guard guard (m_lock);

    AddImpl (std::move (process));
}

void WaitableProcessGroup::Add (std::vector<ProcessRef>&& processes)
{
    std::lock_guard guard (m_lock);

    for (auto& process : processes)
        AddImpl (std::move (process));
}

bool WaitableProcessGroup::Delete (PID pid)
{
    std::lock_guard guard (m_lock);

    if (!m_processes.contains (pid))
        return false;

    const ProcessRef& process = m_processes.at (pid);
    ProcessWaitContext& waitContext = m_waitContexts[&process];

    if (!waitContext.finished && waitContext.hWaitHandle != nullptr) {
        ETWP_ASSERT (waitContext.pCallbackContext != nullptr);
        // The wait callback might already be running, so we need to cancel it, and wait for its completion
        //   synchronously, before actually deleting the ProcessRef
        waitContext.pCallbackContext->canceledEvent.Set ();
        CancelWait (&waitContext, true);
    }

    m_waitContexts.erase (&process);
    m_processes.erase (pid);

    if (IsAllFinished ())    // It's possible that we've just deleted the very last "outstanding" process
        m_allFinishedEvent.Set ();

    return true;
}

size_t WaitableProcessGroup::GetSize () const
{
    std::lock_guard guard (m_lock);

    return m_processes.size ();
}

bool WaitableProcessGroup::WaitForAll (Timeout timeout/* = Infinite*/)
{
    m_lock.lock ();

    if (m_processes.empty ()) {
        m_lock.unlock ();

        return true;
    }

    EnsureWaitingForAll ();
    
    m_lock.unlock ();

    return m_allFinishedEvent.Wait (timeout);
}

bool WaitableProcessGroup::IsAllFinished ()
{
    std::lock_guard guard (m_lock);

    EnsureWaitingForAll ();

    for (const auto& [_, waitContext] : m_waitContexts) {
        if (!waitContext.finished)
            return false;
    }

    return true;
}

WaitableProcessGroup::ConstIterator WaitableProcessGroup::begin () const
{
    std::lock_guard guard (m_lock);

    return { m_processes.cbegin (), this };
}

WaitableProcessGroup::ConstIterator WaitableProcessGroup::end () const
{
    std::lock_guard guard (m_lock);

    return { m_processes.cend (), this };
}

void WaitableProcessGroup::ProcessWaitCallback (PVOID context, BOOLEAN timeout)
{
    ETWP_ASSERT (context != nullptr);
    ETWP_ASSERT (!timeout);  // Since we always wait for an infinite amount, this should not happen
    ETWP_UNUSED_VARIABLE (timeout); // To make Release builds happy...

    Impl::WaitCallbackContext& callbackContext = *static_cast<Impl::WaitCallbackContext*> (context);
    WaitableProcessGroup* pParent = callbackContext.pParent;

    // QUIRK: Unfortunately, RegisterWaitForSingleObject does not provide a mechanism to detect cancellation *after*
    //   the callback has already started running. This is problematic, becuase the following situation can occur:
    //   1.) Thread (A) obtains the process group lock
    //   2.) A process finishes, the callback for it starts runnning on thread (B)
    //   3.) Thread (A) waits for the callback to finish, synchronously
    //   ->  (B) cannot proceed, as it would need the process group lock, which (A) holds. Deadlock!
    //
    //  We "work around" this limitation by periodically trying to acquire the lock, while also checking a
    //   cancellation event. This is not optimal, as: - if there is contention, this will incur some extra CPU overhead
    //                                                  and delay the callback
    //                                                - the detection of cancellation can be delayed up to the time we
    //                                                  wait for the event
    //                                                - if a thread is waiting synchronously for this callback to be
    //                                                  cancelled, all other callbacks will be blocked as well until
    //                                                  the cancellation is "detected", since the lock is held by the
    //                                                  waiting thread
    bool lockAcquired = false;
    do {
        if (callbackContext.canceledEvent.IsSet ())
            return;

        lockAcquired = pParent->m_lock.try_lock_for (100ms);
    } while (!lockAcquired);

    OnExit lockReleaser ([&pParent] () { pParent->m_lock.unlock (); });

    const ProcessRef* pProcess = callbackContext.pProcess;
    ProcessWaitContext& waitContext = pParent->m_waitContexts.at (pProcess);

    waitContext.finished = true;

    if (pParent->IsAllFinished ())
        pParent->m_allFinishedEvent.Set ();

    CancelWait (&waitContext, false);   // Weird, but according to the docs this is needed even for WT_EXECUTEONLYONCE
}

void WaitableProcessGroup::CancelWait (ProcessWaitContext* pWaitContext, bool waitForCallback)
{
    if (pWaitContext->hWaitHandle == nullptr)
        return; // Wait not in progress (did not start yet, or process has finished), nothing to cancel

    pWaitContext->pCallbackContext->canceledEvent.Set ();

    if (waitForCallback)
        std::ignore = UnregisterWaitEx (pWaitContext->hWaitHandle, INVALID_HANDLE_VALUE);   // Blocking wait
    else
        std::ignore = UnregisterWait (pWaitContext->hWaitHandle);

    pWaitContext->hWaitHandle = nullptr;
}

void WaitableProcessGroup::AddImpl (ProcessRef&& process)
{
    ETWP_ASSERT (process.GetOptions () & ProcessRef::Synchronize);

    const PID pid = process.GetPID ();
    const HANDLE handle = process.GetHandle ();
    ETWP_ASSERT (!m_processes.contains (pid));

    m_processes.emplace (pid, std::move(process));
    m_waitContexts.insert ({ &m_processes.at (pid) , { handle, nullptr, {} } });

    m_allFinishedEvent.Reset ();
}

void WaitableProcessGroup::EnsureWaitingForAll ()
{
    for (const auto& [pid, process] : m_processes) {
        ProcessWaitContext& waitContext = m_waitContexts.at (&process);

        // If wait is already in progress, or process has finished, no need to start waiting
        if (waitContext.finished || waitContext.hWaitHandle != nullptr)
            continue;

        waitContext.pCallbackContext = std::make_unique<Impl::WaitCallbackContext> (this, &process);

        ETWP_VERIFY (RegisterWaitForSingleObject (&waitContext.hWaitHandle,
                                                  process.GetHandle (),
                                                  ProcessWaitCallback,
                                                  (PVOID)waitContext.pCallbackContext.get (),
                                                  INFINITE,
                                                  WT_EXECUTEONLYONCE));
    }
}

}   // namespace ETWP