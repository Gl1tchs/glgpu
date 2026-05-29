#pragma once

#include <catch2/catch_test_macros.hpp>
#include <string>

#include "gpukit/device.h"

namespace gpukit::test {

inline DeviceHandle g_test_device = GL_NULL_HANDLE;

inline void ensure_test_device() {
	if (g_test_device != GL_NULL_HANDLE)
		return;

	DeviceCreateInfo info = {};
	info.required_features = DEVICE_FEATURE_VALIDATION_LAYERS;

	auto res = init(info);
	if (!res) {
		fprintf(stderr, "FATAL: Could not initialize Vulkan Backend. Error: %d\n",
				(int)res.error());
		std::terminate();
	}

	g_test_device = res.value();
}

inline void destroy_test_device() {
	if (g_test_device == GL_NULL_HANDLE)
		return;
	select_device(g_test_device);
	shutdown(g_test_device);
	g_test_device = GL_NULL_HANDLE;
}

inline Res<Shader> load_shader(const char* vert, const char* frag) {
	auto try_prefix = [&](const char* prefix) {
		return shader_create(
				(std::string(prefix) + vert).c_str(),
				(std::string(prefix) + frag).c_str());
	};
	auto res = try_prefix("tests/assets/");
	if (res.is_error())
		res = try_prefix("../../tests/assets/");
	return res;
}

inline Res<Shader> load_compute_shader(const char* comp) {
	auto res = shader_create(("tests/assets/" + std::string(comp)).c_str());
	if (res.is_error())
		res = shader_create(("../../tests/assets/" + std::string(comp)).c_str());
	return res;
}

} // namespace gpukit::test
