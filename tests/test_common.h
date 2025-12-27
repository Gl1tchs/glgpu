#pragma once

#include <catch2/catch_test_macros.hpp>

#include "glgpu/backend.h"

namespace gl::test {

// Internal storage for the singleton
inline std::shared_ptr<RenderBackend>& _get_backend_storage() {
	static std::shared_ptr<RenderBackend> backend = nullptr;
	return backend;
}

/**
 * Returns the shared backend instance. Creates it if it doesn't exist.
 */
inline std::shared_ptr<RenderBackend> get_test_backend() {
	auto& backend = _get_backend_storage();

	if (!backend) {
		RenderBackendCreateInfo info = {};
		info.api = RenderAPI::VULKAN;
		info.required_features = RENDER_BACKEND_FEATURE_NONE;
		info.native_window_handle = nullptr;
		info.native_connection_handle = nullptr;

		auto res = RenderBackend::create(info);

		if (res.is_error()) {
			fprintf(stderr, "FATAL: Could not initialize Vulkan Backend. Error: %d\n",
					(int)res.error());
			std::terminate();
		}

		backend = res.value();
	}

	return backend;
}

/**
 * Manually destroys the backend instance.
 */
inline void destroy_test_backend() { _get_backend_storage().reset(); }

} // namespace gl::test
