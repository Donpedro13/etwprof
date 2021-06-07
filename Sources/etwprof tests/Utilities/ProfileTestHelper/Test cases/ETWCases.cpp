#include "OperationRegistrar.hpp"

#include <windows.h>
#include <TraceLoggingProvider.h>

#include <mutex>

#include "ETW providers/Manifest based/MB.h"

namespace PTH {
namespace {

using Template1Func = std::function<void (const WCHAR* pStr, UINT16 num, const FILETIME& fileTime, const GUID& guid)>;

// 11b83188-f8a1-5301-5690-e964fd71beba
TRACELOGGING_DEFINE_PROVIDER (gTLA,
                              "TLA",
                              (0x11b83188, 0xf8a1, 0x5301, 0x56, 0x90, 0xe9, 0x64, 0xfd, 0x71, 0xbe, 0xba));

// 7ae7cc76-bdaf-5e8a-1b73-d85398dbadd3
TRACELOGGING_DEFINE_PROVIDER (gTLB,
                              "TLB",
                              (0x7ae7cc76, 0xbdaf, 0x5e8a, 0x1b, 0x73, 0xd8, 0x53, 0x98, 0xdb, 0xad, 0xd3));

const FILETIME& GetDummyFileTime()
{
    static FILETIME DummyFileTime = {};
    static std::once_flag of;

    auto setFileTime = [] () {
        GetSystemTimeAsFileTime (&DummyFileTime);
    };

    std::call_once (of, setFileTime);

    return DummyFileTime;
}

class RegistratorHelper {
public:
    using RegUnregFunc = std::function<void ()>;

    RegistratorHelper (const RegUnregFunc& fReg, const RegUnregFunc& fUnreg) : m_unregFunc (fUnreg)
    {
        fReg ();
    }
private:
    RegUnregFunc m_unregFunc;
};

void HandleRegUnregMBA ()
{
    static RegistratorHelper helper ([] () { EventRegisterMBA (); }, [] () { EventUnregisterMBA (); });
}

void HandleRegUnregMBB ()
{
    static RegistratorHelper helper ([] () { EventRegisterMBB (); }, [] () { EventUnregisterMBB (); });
}

void WriteEventWithTemplate1 (const Template1Func& f)
{
    f (L"TEST", 42, GetDummyFileTime (), MB_A);
}

void EmitAllMBA ()
{
    HandleRegUnregMBA ();

    WriteEventWithTemplate1 ([] (const WCHAR* pStr, UINT16 num, const FILETIME& fileTime, const GUID& guid) {
        EventWriteA_EventA (pStr, num, &fileTime, &guid);
        EventWriteA_EventB (pStr, num, &fileTime, &guid);
    });
    
    EventWriteA_EventC (TRUE);
}

void EmitAllMBB ()
{
    HandleRegUnregMBA ();

    WriteEventWithTemplate1 ([] (const WCHAR* pStr, UINT16 num, const FILETIME& fileTime, const GUID& guid) {
        EventWriteB_EventA (pStr, num, &fileTime, &guid);
        EventWriteB_EventB (pStr, num, &fileTime, &guid);
    });
    
    EventWriteB_EventC (FALSE);
}

void HandleRegUnregTLA ()
{
    static RegistratorHelper helper ([] () { TraceLoggingRegister (gTLA); }, [] () { TraceLoggingUnregister (gTLA); });
}

void HandleRegUnregTLB ()
{
    static RegistratorHelper helper ([] () { TraceLoggingRegister (gTLB); }, [] () { TraceLoggingUnregister (gTLB); });
}

void EmitAllTLA ()
{
    HandleRegUnregTLA ();

    WriteEventWithTemplate1 ([] (const WCHAR* pStr, UINT16 num, const FILETIME& fileTime, const GUID& guid) {
        TraceLoggingWrite (gTLA,
                           "TLA_EventA",
                           TraceLoggingKeyword (0b01),
                           TraceLoggingLevel (TRACE_LEVEL_CRITICAL),
                           TraceLoggingStruct (4, "TestStructA"),
                           TraceLoggingWideString (pStr),
                           TraceLoggingUInt16 (num),
                           TraceLoggingFileTime (fileTime),
                           TraceLoggingGuid (guid));
    });

    TraceLoggingWrite (gTLA,
                       "TLA_EventB",
                       TraceLoggingKeyword (0b10),
                       TraceLoggingLevel (TRACE_LEVEL_WARNING),
                       TraceLoggingBool (TRUE));
}

void EmitAllTLB ()
{
    HandleRegUnregTLB ();

    WriteEventWithTemplate1 ([] (const WCHAR* pStr, UINT16 num, const FILETIME& fileTime, const GUID& guid) {
        TraceLoggingWrite (gTLB,
                           "TLB_EventA",
                           TraceLoggingKeyword (0b10),
                           TraceLoggingLevel (TRACE_LEVEL_WARNING),
                           TraceLoggingStruct (4, "TestStructB"),
                           TraceLoggingWideString (pStr),
                           TraceLoggingUInt16 (num),
                           TraceLoggingFileTime (fileTime),
                           TraceLoggingGuid (guid));
    });

    TraceLoggingWrite (gTLB,
                       "TLB_EventB",
                       TraceLoggingKeyword (0b01),
                       TraceLoggingLevel (TRACE_LEVEL_CRITICAL),
                       TraceLoggingBool (FALSE));
}

static OperationRegistrator registrator1 (L"TLEmitA", []() {
    EmitAllTLA ();

    return true;
});

static OperationRegistrator registrator2 (L"TLEmitB", [] () {
    EmitAllTLB ();

    return true;
});

static OperationRegistrator registrator3 (L"TLEmitAB", [] () {
    EmitAllTLA ();
    EmitAllTLB ();

    return true;
});

static OperationRegistrator registrator4 (L"MBEmitA", []() {
    EmitAllMBA ();

    return true;
});

static OperationRegistrator registrator5 (L"MBEmitB", [] () {
    EmitAllMBB ();

    return true;
});

static OperationRegistrator registrator6 (L"MBEmitAB", [] () {
    EmitAllMBA ();
    EmitAllMBB ();

    return true;
});

}	// namespace
}	// namespace PTH