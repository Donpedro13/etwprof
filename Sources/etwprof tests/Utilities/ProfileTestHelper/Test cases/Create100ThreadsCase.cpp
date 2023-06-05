#include "OperationRegistrar.hpp"

#include <process.h>
#include <windows.h>

#include <vector>

namespace PTH {
namespace {

unsigned int ThreadFunc (void*)
{
    return 0;
}

bool Create100ThreadsCase ()
{
    std::vector<HANDLE> threads;
    for (auto i = 0; i < 100; ++i) {
        HANDLE hThread;
        hThread = reinterpret_cast<HANDLE> (_beginthreadex(nullptr, 0, ThreadFunc, nullptr, 0, nullptr));
        if (hThread == nullptr)
            return false;   // We may potentially "leak" some thread handles, but the process will soon exit anyway...

        threads.push_back(hThread);

        Sleep(1);
    }

    for (auto i = 0; i < 100; ++i) {
        switch (WaitForSingleObject(threads[i], INFINITE)) {
            case WAIT_OBJECT_0:
                break;
            case WAIT_FAILED:
                [[fallthrough]];
            default:
                return false;
        }

        CloseHandle (threads[i]);
    }

    return true;
}

OperationRegistrator testCaseRegistrator (L"Create100Threads", []() { return Create100ThreadsCase (); });

}	// namespace
}	// namespace PTH