#include <catch2/catch_test_macros.hpp>

#include "test_common.h"

using namespace gpukit;

// Uses test_compute.comp which has set=0, binding=0 as STORAGE_BUFFER.
TEST_CASE("Uniform Set Extended", "[uniform]") {
	gpukit::test::ensure_test_device();

	auto shader_res = gpukit::test::load_compute_shader("test_compute.comp");
	REQUIRE(shader_res.is_ok());
	Shader shader = shader_res.value();

	SECTION("Create with explicit ShaderUniform list (STORAGE_BUFFER)") {
		auto buf_res = buffer_create(256, BUFFER_USAGE_STORAGE_BUFFER_BIT, MemoryAllocationType::CPU);
		REQUIRE(buf_res.is_ok());
		Buffer buf = buf_res.value();

		// data[0] is the Buffer handle cast to void* — the implementation reads it as
		// a VulkanBuffer* to fill the VkDescriptorBufferInfo.
		ShaderUniform uniform = {};
		uniform.type = ShaderUniformType::STORAGE_BUFFER;
		uniform.binding = 0;
		uniform.data = { (void*)buf };

		auto set_res = uniform_set_create(uniform, shader, 0);
		REQUIRE(set_res.is_ok());
		UniformSet set = set_res.value();
		REQUIRE(set != GL_NULL_HANDLE);

		REQUIRE(uniform_set_free(set).is_ok());
		REQUIRE(buffer_free(buf).is_ok());
	}

	SECTION("uniform_set_update_buffer_range: partial range binding") {
		// Create a buffer large enough to hold multiple elements and bind only a slice.
		constexpr uint64_t BUF_SIZE = 1024;
		constexpr uint64_t OFFSET = 256;
		constexpr uint64_t RANGE = 256;

		auto buf_res = buffer_create(BUF_SIZE, BUFFER_USAGE_STORAGE_BUFFER_BIT, MemoryAllocationType::CPU);
		REQUIRE(buf_res.is_ok());
		Buffer buf = buf_res.value();

		auto set_res = uniform_set_create(shader, 0);
		REQUIRE(set_res.is_ok());
		UniformSet set = set_res.value();

		REQUIRE(uniform_set_update_buffer_range(set, 0, 0, buf, OFFSET, RANGE).is_ok());

		REQUIRE(uniform_set_free(set).is_ok());
		REQUIRE(buffer_free(buf).is_ok());
	}

	REQUIRE(shader_free(shader).is_ok());
}
