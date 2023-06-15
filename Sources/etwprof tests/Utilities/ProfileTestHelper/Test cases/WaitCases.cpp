#include "OperationRegistrar.hpp"

#include <windows.h>

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

}	// namespace
}	// namespace PTH