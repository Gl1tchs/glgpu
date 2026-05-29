#define CATCH_CONFIG_RUNNER
#include <catch2/catch_session.hpp>

#include <iostream>
#include <string>

#include "test_common.h"

#if defined(GPUKIT_LSAN_SUPPRESSIONS_PATH)
extern "C" const char* __lsan_default_options() {
	return "suppressions=" GPUKIT_LSAN_SUPPRESSIONS_PATH;
}
#endif
#if defined(GPUKIT_UBSAN_SUPPRESSIONS_PATH)
extern "C" const char* __ubsan_default_options() {
	return "suppressions=" GPUKIT_UBSAN_SUPPRESSIONS_PATH ":print_stacktrace=1";
}
#endif

int main(int argc, char* argv[]) {
	Catch::Session session;

	int code = session.applyCommandLine(argc, argv);
	if (code != 0)
		return code;

	bool is_listing = false;
	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		if (arg == "--list-tests" || arg == "--list-tags" || arg == "--list-reporters" ||
				arg == "--list-listeners" || arg == "-h" || arg == "--help" || arg == "-?") {
			is_listing = true;
			break;
		}
	}

	if (is_listing)
		return session.run();

	std::cout << "[Compute Tests] Initializing device..." << std::endl;
	gpukit::test::ensure_test_device();

	std::cout << "[Compute Tests] Running tests..." << std::endl;
	const int failed = session.run();

	std::cout << "[Compute Tests] Tearing down device..." << std::endl;
	gpukit::device_wait();
	gpukit::test::destroy_test_device();

	return failed;
}
