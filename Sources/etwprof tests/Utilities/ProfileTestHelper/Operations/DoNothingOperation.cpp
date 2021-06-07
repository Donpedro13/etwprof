#include "OperationRegistrar.hpp"

namespace PTH {
namespace {

static OperationRegistrator registrator (L"DoNothing", []() {
	return true;    // Well, do nothing...
});


}	// namespace
}	// namespace PTH