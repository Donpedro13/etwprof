#include "OperationRegistrar.hpp"

#include <cstdint>
#include <chrono>

namespace PTH {
namespace {

void BurnCPU (uint32_t durationSec)
{
    using namespace std::chrono;

    const uint32_t durationMilliSec = durationSec * 1'000;
    auto start = system_clock::now ();

    do {
        // Burn...
    } while (duration_cast<milliseconds> (system_clock::now () - start).count () < durationMilliSec);
}

__declspec(noinline) void HelperB ()
{
    BurnCPU (2);
}

__declspec(noinline) void HelperA ()
{
	HelperB ();
    BurnCPU (1);
}

__declspec(noinline) void BurnCPU5s ()
{
    BurnCPU (1);

    HelperA ();

    BurnCPU (1);
}

static OperationRegistrator registrator (L"BurnCPU5s", []() {
    BurnCPU5s ();
    
    return true;
});

}	// namespace
}	// namespace PTH