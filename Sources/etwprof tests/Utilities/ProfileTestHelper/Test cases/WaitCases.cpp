#include <windows.h>

#include "OperationRegistrar.hpp"
#include "Utility.hpp"

namespace PTH {
namespace {

void Wait (uint16_t waitSec)
{
    Sleep (waitSec * 1'000);
}

static OperationRegistrator registrator1 (L"Wait1Sec", []() {
    Wait (1);

    return true;
});

static OperationRegistrator registrator2 (L"Wait5Sec", []() {
    Wait (5);

    return true;
});

static OperationRegistrator registrator3 (L"WaitForParent", []() {
    DWORD parentPID = 0;
    if (!GetParentPID (&parentPID))
        return true;    // The parent process must have exited already, no need to wait

    WinHandle hParent = OpenProcess (SYNCHRONIZE, FALSE, parentPID);
    if (hParent.Get () == nullptr)
        return true;    // The parent process must have exited already, no need to wait
    
    return WaitForProcess (hParent);
});

}	// namespace
}	// namespace PTH