#include <iostream>
#include <string>

#define CATCH_CONFIG_RUNNER
#include <catch2/catch_session.hpp>

#include "test_common.h"

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

	// Initialize the RenderBackend globally
	std::cout << "[Test Entry] Initializing RenderBackend..." << std::endl;
	auto device = gpukit::test::get_test_device();

	if (!device) {
		std::cerr << "[Test Entry] Failed to create backend. Aborting." << std::endl;
		return 1;
	}

	// Run tests
	std::cout << "[Test Entry] Running tests..." << std::endl;

	const int num_failed = session.run();

	// Cleanup
	std::cout << "[Test Entry] Destroying RenderBackend..." << std::endl;
	gpukit::test::destroy_test_device();

	return num_failed;
}
