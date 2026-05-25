#pragma once

#include <catch2/catch_test_macros.hpp>

#include "gpukit/device.h"

namespace gpukit::test {

/**
 * Initialize the backend if not already done. Terminates on failure.
 */
inline void get_test_device() {
	static bool initialized = false;
	if (initialized)
		return;

	DeviceCreateInfo info = {};
	info.required_features = DEVICE_FEATURE_VALIDATION_LAYERS;

	if (auto res = init(info); !res) {
		fprintf(stderr, "FATAL: Could not initialize Vulkan Backend. Error: %d\n",
				(int)res.error());
		std::terminate();
	}

	initialized = true;
}

/**
 * Shuts down the backend.
 */
inline void destroy_test_device() { shutdown(); }

} // namespace gpukit::test
