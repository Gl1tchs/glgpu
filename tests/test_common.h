#pragma once

#include <catch2/catch_test_macros.hpp>

#include "gpukit/device.h"

namespace gpukit::test {

// Internal storage for the singleton
inline std::unique_ptr<Device>& _get_device_storage() {
	static std::unique_ptr<Device> device = nullptr;
	return device;
}

/**
 * Returns the unique backend instance. Creates it if it doesn't exist.
 */
inline Device* get_test_device() {
	auto& device = _get_device_storage();

	if (!device) {
		DeviceCreateInfo info = {};
		info.api = RenderAPI::VULKAN;
		info.required_features = DEVICE_FEATURE_VALIDATION_LAYERS;
		info.native_window_handle = nullptr;
		info.native_connection_handle = nullptr;

		auto res = Device::create(info);

		if (res.is_error()) {
			fprintf(stderr, "FATAL: Could not initialize Vulkan Backend. Error: %d\n",
					(int)res.error());
			std::terminate();
		}

		device = std::move(res.value());
	}

	return device.get();
}

/**
 * Manually destroys the backend instance.
 */
inline void destroy_test_device() { _get_device_storage().reset(); }

} // namespace gpukit::test
