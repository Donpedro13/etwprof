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
    using Size = uint16_t;   // DWORD, used of PIDs on Windows

    class ConstIterator {    // Needed for atomic iteration
    public:
        using base_iterator = ProcessesByPIDMap::const_iterator;

        using iterator_category = base_iterator::iterator_category;
        using difference_type = base_iterator::difference_type;
        using value_type = base_iterator::value_type;
        using pointer = base_iterator::pointer;
        using reference = base_iterator::reference;
        
        ConstIterator (const base_iterator& origIt, const WaitableProcessGroup* pParent);
        ConstIterator (const ConstIterator& rhs);
        ConstIterator (const ConstIterator&& rhs) noexcept;
        ~ConstIterator ();

        reference operator*() const { return *m_baseIt; }
        pointer operator->() { return m_baseIt.operator->(); }

        ConstIterator& operator++() { m_baseIt++; return *this; }
        ConstIterator operator++ (int) { ConstIterator tmp = *this; ++(*this); return tmp; }

        friend bool operator== (const ConstIterator& a, const ConstIterator& b) { return a.m_baseIt == b.m_baseIt; };
        friend bool operator!= (const ConstIterator& a, const ConstIterator& b) { return a.m_baseIt != b.m_baseIt; };

    private:
        const WaitableProcessGroup* m_pParent;
        base_iterator m_baseIt;

    };

    WaitableProcessGroup (std::vector<ProcessRef>&& processes);
    WaitableProcessGroup () = default;

    WaitableProcessGroup (WaitableProcessGroup&& other);    // Declared for (N)RVO, but not defined

    ~WaitableProcessGroup ();

    void Add (ProcessRef&& process);
    void Add (std::vector<ProcessRef>&& processes);
    bool Delete (PID pid);

    Size GetSize () const;
    Size GetWaitingSize () const;
    Size GetFinishedSize () const;

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