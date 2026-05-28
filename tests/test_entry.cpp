#include <iostream>
#include <string>

#define CATCH_CONFIG_RUNNER
#include <catch2/catch_session.hpp>

#include "test_common.h"

// Sanitizer hook functions — called by the runtime during initialization,
// before ASAN_OPTIONS / LSAN_OPTIONS / UBSAN_OPTIONS env vars are processed.
// Paths are set at compile time by CMake when GPUKIT_ENABLE_SANITIZERS is ON.
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
	if (code != 0) {
		return code;
	}

	bool is_listing = false;
	for (int i = 1; i < argc; ++i) {
		std::string arg = argv[i];
		if (arg == "--list-tests" || arg == "--list-tags" || arg == "--list-reporters" ||
				arg == "--list-listeners" || arg == "-h" || arg == "--help" || arg == "-?") {
			is_listing = true;
			break;
		}
	}

	if (is_listing) {
		return session.run();
	}

	std::cout << "[Test Entry] Initializing RenderBackend..." << std::endl;
	gpukit::test::ensure_test_device();

	std::cout << "[Test Entry] Running tests..." << std::endl;
	const int num_failed = session.run();

	std::cout << "[Test Entry] Destroying RenderBackend..." << std::endl;
	gpukit::device_wait();
	gpukit::test::destroy_test_device();

	return num_failed;
}
