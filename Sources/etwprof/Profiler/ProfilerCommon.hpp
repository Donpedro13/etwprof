#ifndef ETWP_PROFILER_COMMON_HPP
#define ETWP_PROFILER_COMMON_HPP

#include <windows.h>

#include <string>
#include <unordered_set>

struct ITraceEvent;
class TraceRelogger;

namespace ETWP {

struct ProfileFilterData {
    std::unordered_set<DWORD> threads;
    DWORD targetPID;
    bool  cswitch;
};

// context is a pointer to a ProfileFilterData instance
void FilterEventForProfiling (ITraceEvent* pEvent, TraceRelogger* pRelogger, void* context);

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