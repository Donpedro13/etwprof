#include "CriticalSection.hpp"

#include "Utility/Macros.hpp"

namespace ETWP {

CriticalSection::CriticalSection (DWORD spinCount /*= 0*/)
{
    // I think we might be able to call InitializeCriticalSectionAndSpinCount in
    //   both cases, but I'm not entirely sure...
    if (spinCount == 0) {
        InitializeCriticalSection (&m_cs);
    } else {
        BOOL result = InitializeCriticalSectionAndSpinCount (&m_cs, spinCount);
        // According to the documentation, the return value is always zero since
        //   Windows Vista, yet the function is marked with
        //   _Must_inspect_result_. This makes /analyze happy.
        ETWP_UNUSED_VARIABLE (result != FALSE);
    }
}

CriticalSection::~CriticalSection ()
{
    DeleteCriticalSection (&m_cs);
}

void CriticalSection::Lock ()
{
    EnterCriticalSection (&m_cs);
}

bool CriticalSection::TryLock ()
{
    return TryEnterCriticalSection (&m_cs) != FALSE;
}

void CriticalSection::Unlock ()
{
    LeaveCriticalSection (&m_cs);
}

}   // namespace ETWP