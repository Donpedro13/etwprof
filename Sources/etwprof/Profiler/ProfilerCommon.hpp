#ifndef ETWP_PROFILER_COMMON_HPP
#define ETWP_PROFILER_COMMON_HPP

#include <windows.h>

#include <string>
#include <unordered_map>
#include <unordered_set>

#include "IETWBasedProfiler.hpp"

#include "OS/ETW/TraceRelogger.hpp"

#include "OS/Utility/Time.hpp"

struct ITraceEvent;

template<>
struct std::hash<ETWP::IETWBasedProfiler::ProviderInfo> {
    std::size_t operator() (const ETWP::IETWBasedProfiler::ProviderInfo& providerInfo) const {
        const GUID& guid = providerInfo.providerID;
        // According to my experiments (with GUIDs generated with CoCreateGuid), Data1-3 used as a hash value provides
        //   significantly better uniformity than Data4. I'm not sure why, and haven't bothered to investigate
        return *reinterpret_cast<const std::size_t*> (&guid.Data1);
    }
};

namespace ETWP {

// Class for recording thread IDs of interest. Besides deleting, you can also mark threads for deletion. These can be
//   deleted later manually by calling DeleteThreadsMarkedForDeletion, with a time threshold
class ThreadRegistry {
public:
    void Add (DWORD tid);
    void MarkForDeletion (DWORD tid);
    void Delete (DWORD tid);
    bool Contains (DWORD tid) const;

    void DeleteThreadsMarkedForDeletion (TickCount markTimeThreshold);

private:
    std::unordered_set<DWORD>            m_threadIDs;
    std::unordered_map<DWORD, TickCount> m_threadIDsMarkedForDeletion;    // TID + tick count when it was marked for deletion
};

inline void ThreadRegistry::Add (DWORD tid)
{
    m_threadIDs.insert (tid);
}

inline void ThreadRegistry::MarkForDeletion (DWORD tid)
{
    m_threadIDsMarkedForDeletion.insert({ tid, GetTickCount ()});
}

inline void ThreadRegistry::Delete (DWORD tid)
{
    m_threadIDs.erase (tid);
    m_threadIDsMarkedForDeletion.erase (tid);
}

inline bool ThreadRegistry::Contains (DWORD tid) const
{
    return m_threadIDs.contains (tid);
}

struct ProfileFilterData {
    // Unfortunately, sometimes we receive events for a thread *after* their end event. To mitigate this, we keep
    //   terminated thread ID's around for some time.
    ThreadRegistry threads;
    // In theory, std::vector should perform better if the number of providers is small. However, my experiments showed
    //   only a very tiny difference, so using a hash set alone should suffice
    std::unordered_set<IETWBasedProfiler::ProviderInfo> userProviders;
    std::unordered_set<UINT_PTR> stackKeys;  // For stack cache filtering (when enabled)
    std::unordered_set<DWORD> targetPIDs;
    bool  cswitch;
};

class ProfileEventFilter final : public IEventFilter {
public:
    ProfileEventFilter (ProfileFilterData& filterData);

    virtual void FilterEvent (ITraceEvent* pEvent, TraceRelogger* pRelogger) override;

private:
    ProfileFilterData& m_filterData;
};

void FilterEventForProfiling (ITraceEvent* pEvent,
                              TraceRelogger* pRelogger,
                              ProfileFilterData* pFilterData);

// This code snippet is copied here from KernelTraceControl.h in the Windows SDK
#define EVENT_TRACE_MERGE_EXTENDED_DATA_NONE                0x00000000
#define EVENT_TRACE_MERGE_EXTENDED_DATA_IMAGEID             0x00000001
#define EVENT_TRACE_MERGE_EXTENDED_DATA_BUILDINFO           0x00000002
#define EVENT_TRACE_MERGE_EXTENDED_DATA_VOLUME_MAPPING      0x00000004
#define EVENT_TRACE_MERGE_EXTENDED_DATA_WINSAT              0x00000008
#define EVENT_TRACE_MERGE_EXTENDED_DATA_EVENT_METADATA      0x00000010
#define EVENT_TRACE_MERGE_EXTENDED_DATA_PERFTRACK_METADATA  0x00000020
#define EVENT_TRACE_MERGE_EXTENDED_DATA_NETWORK_INTERFACE   0x00000040
#define EVENT_TRACE_MERGE_EXTENDED_DATA_NGEN_PDB            0x00000080

#define EVENT_TRACE_MERGE_EXTENDED_DATA_COMPRESS_TRACE      0x10000000
#define EVENT_TRACE_MERGE_EXTENDED_DATA_INJECT_ONLY         0x40000000

#define EVENT_TRACE_MERGE_EXTENDED_DATA_DEFAULT             0x000FFFFF
#define EVENT_TRACE_MERGE_EXTENDED_DATA_ALL                 0x0FFFFFFF

bool MergeTrace (const std::wstring& inputETLPath,
                 DWORD flags,
                 const std::wstring& outputETLPath,
                 std::wstring* pErrorOut);

}   // namespace ETWP

#endif  // #ifndef #define ETWP_PROFILER_COMMON_HPP