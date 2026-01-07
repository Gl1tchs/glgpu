#pragma once

#include <catch2/catch_test_macros.hpp>

#include "glgpu/device.h"

namespace gl::test {

// Internal storage for the singleton
inline std::shared_ptr<Device>& _get_device_storage() {
	static std::shared_ptr<Device> device = nullptr;
	return device;
}

/**
 * Returns the shared backend instance. Creates it if it doesn't exist.
 */
inline std::shared_ptr<Device> get_test_device() {
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

		device = res.value();
	}

	return device;
}

/**
 * Manually destroys the backend instance.
 */
inline void destroy_test_device() { _get_device_storage().reset(); }

} // namespace gl::test
