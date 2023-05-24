#include "OperationRegistrar.hpp"

namespace PTH {
namespace {

static OperationRegistrator registrator (L"DoNothing", []() {
	// Even though this operation's name is "do nothing", we burn some CPU anyway. If we don't, many test cases become
	//   flaky, as sometimes the process finishes so quickly, that the default 1 kHz sample rate does not "capture"
	//   any sampled profile events (especially in release builds)
	volatile unsigned int i = 0;
	while (i != 1'000'000)
		++i;

	return true;
});


}	// namespace
}	// namespace PTH