#ifndef ETWP_ETW_CONSTANTS_HPP
#define ETWP_ETW_CONSTANTS_HPP

// Declare GUIDs in the global namespace
#include "ETWGUIDImpl.inl"

namespace ETWP {
namespace ETWConstants {

// Thread opcodes
const UCHAR TStartOpcode = 1;
const UCHAR TEndOpcode = 2;
const UCHAR TDCStartOpcode = 3;
const UCHAR TDCEndOpcode = 4;
const UCHAR CSwitchOpcode = 36;
const UCHAR ReadyThreadOpcode = 50;

// Process opcodes
const UCHAR PStartOpcode = 1;
const UCHAR PEndOpcode = 2;
const UCHAR PDCStartOpcode = 3;
const UCHAR PDCEndOpcode = 4;

// PerfInfo opcodes
const UCHAR SampledProfileOpcode = 46;

// EventTraceEvent opcodes
const UCHAR RDComplete = 8;

struct ThreadDataStub {
    DWORD m_processID;
    DWORD m_threadID;
    // Other members follow in the "real" struct
};

struct ReadyThreadDataStub {
    DWORD m_readyThreadID;
    UCHAR m_adjustReason;
    UCHAR m_adjustIncrement;
    UCHAR m_flag;
    UCHAR m_reserved;
    // Other members follow in the "real" struct
};

struct CSwitchDataStub {
    DWORD m_newThreadID;
    DWORD m_oldThreadID;
    // Other members follow in the "real" struct
};

struct SampledProfileDataStub {
    UINT_PTR m_ip;
    DWORD    m_threadID;
    // Other members follow in the "real" struct
};

struct ProcessDataStub {
    UINT_PTR m_processKey;
    DWORD    m_processID;
    // Other members follow in the "real" struct
};

struct ImageLoadDataStub {
    UINT_PTR m_imageBase;
    UINT_PTR m_imageSize;
    DWORD    m_processID;
    // Other members follow in the "real" struct
};

struct StackWalkDataStub {
    UINT64 m_timeStamp;
    DWORD  m_processID;
    DWORD  m_threadID;
    // Other members follow in the "real" struct
};

}   // namespace ETWConstants
}   // namespace ETWP

#endif  // #ifndef ETWP_ETW_CONSTANTS_HPP