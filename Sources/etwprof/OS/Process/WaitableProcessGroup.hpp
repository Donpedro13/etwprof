#ifndef ETWP_PROCESS_GROUP_HPP
#define ETWP_PROCESS_GROUP_HPP

#include <mutex>
#include <unordered_map>

#include "ProcessRef.hpp"

#include "OS/Synchronization/Event.hpp"

#include "OS/Utility/Time.hpp"

namespace ETWP {
namespace Impl { struct WaitCallbackContext; }

// Thread-safe class for operating on a group of ProcessRef objects.
class WaitableProcessGroup final {
public:
    using ProcessesByPIDMap = std::unordered_map<PID, ProcessRef>;

    class ConstIterator : public ProcessesByPIDMap::const_iterator {    // Needed for atomic iteration
    public:
        ConstIterator (ProcessesByPIDMap::const_iterator&& origIt, const WaitableProcessGroup* pParent);
        ~ConstIterator ();

    private:
        const WaitableProcessGroup* m_pParent;
    };

    WaitableProcessGroup (std::vector<ProcessRef>&& processes);
    WaitableProcessGroup () = default;

    WaitableProcessGroup (WaitableProcessGroup&& other);    // Declared for (N)RVO, but not defined

    ~WaitableProcessGroup ();

    void Add (ProcessRef&& process);
    void Add (std::vector<ProcessRef>&& processes);
    bool Delete (PID pid);

    size_t GetSize () const;

    bool WaitForAll (Timeout timeout = Infinite);
    bool IsAllFinished ();

    ConstIterator begin () const;
    ConstIterator end () const;

private:
    struct ProcessWaitContext {
        HANDLE hProcess = nullptr;
        HANDLE hWaitHandle = nullptr;
        std::unique_ptr<Impl::WaitCallbackContext> pCallbackContext;

        bool finished = false;
    };

    static void ProcessWaitCallback (PVOID context, BOOLEAN timeout);                   // Not synchronized
    static void CancelWait (ProcessWaitContext* pWaitContext, bool waitForCallback);    // Not synchronized

    mutable std::recursive_timed_mutex m_lock;

    ProcessesByPIDMap m_processes;

    std::unordered_map<const ProcessRef*, ProcessWaitContext> m_waitContexts;
    Event                                                     m_allFinishedEvent;

    void AddImpl (ProcessRef&& process); // Not synchronized
    void EnsureWaitingForAll ();    // Not synchronized
};

namespace Impl {

struct WaitCallbackContext {
    WaitableProcessGroup* pParent;
    const ProcessRef* pProcess;
    // There is no mechanism provided by RWFSO to detect cancellation when the callback already started,
    //   so we rolled our own...
    Event canceledEvent;
};

}   // namespace Impl

}   // namespace ETWP

#endif  // #ifndef ETWP_PROCESS_GROUP_HPP