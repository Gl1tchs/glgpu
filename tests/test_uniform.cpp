#include "test_common.h"
#include <catch2/catch_test_macros.hpp>
#include <fstream>
#include <vector>

using namespace gl;

static std::vector<uint32_t> test_load_spirv(const std::string& path1, const std::string& path2) {
	std::ifstream file(path1, std::ios::binary | std::ios::ate);
	if (!file.is_open()) {
		file.open(path2, std::ios::binary | std::ios::ate);
	}
	if (!file.is_open()) {
		return {};
	}
	size_t size = file.tellg();
	std::vector<uint32_t> buffer(size / 4);
	file.seekg(0);
	file.read(reinterpret_cast<char*>(buffer.data()), size);
	return buffer;
}

TEST_CASE("Bindless Uniform Management", "[uniform]") {
	auto device = gl::test::get_test_device();
	REQUIRE(device != nullptr);

	// Load bindless shaders
	auto vert_code = test_load_spirv(
			"tests/assets/test_bindless_vert.spv", "../../tests/assets/test_bindless_vert.spv");
	auto frag_code = test_load_spirv(
			"tests/assets/test_bindless_frag.spv", "../../tests/assets/test_bindless_frag.spv");

	REQUIRE(!vert_code.empty());
	REQUIRE(!frag_code.empty());

	SpirvEntry vert_entry{ .byte_code = vert_code, .stage = SHADER_STAGE_VERTEX_BIT };
	SpirvEntry frag_entry{ .byte_code = frag_code, .stage = SHADER_STAGE_FRAGMENT_BIT };
	std::vector<SpirvEntry> entries = { vert_entry, frag_entry };

	auto shader_res = device->shader_create_from_bytecode(entries);
	REQUIRE(shader_res.is_ok());
	Shader shader = shader_res.value();

	SECTION("Create and Update Bindless Descriptor Set") {
		// Create bindless set at set 0, binding 0, max 1000 items
		auto set_res = device->uniform_set_create_bindless(shader, 0, 0, 1000);
		REQUIRE(set_res.is_ok());
		UniformSet set = set_res.value();
		REQUIRE(set != GL_NULL_HANDLE);

		// Create test image and sampler
		ImageCreateInfo img_info = {};
		img_info.size = { 16, 16 };
		img_info.format = DataFormat::R8G8B8A8_UNORM;
		img_info.usage = IMAGE_USAGE_SAMPLED_BIT | IMAGE_USAGE_STORAGE_BIT;
		img_info.samples = 1;
		img_info.mipmapped = false;

		auto img_res = device->image_create(img_info);
		REQUIRE(img_res.is_ok());
		Image img = img_res.value();

		SamplerCreateInfo sampler_info{};
		auto sampler_res = device->sampler_create(sampler_info);
		REQUIRE(sampler_res.is_ok());
		Sampler sampler = sampler_res.value();

		// Test Combined Image Sampler update (binding = 0)
		auto update_tex_res = device->uniform_set_update_texture(set, 0, 0, img, sampler);
		REQUIRE(update_tex_res.is_ok());

		// Test Sampled Image update (binding = 1)
		auto update_sampled_res = device->uniform_set_update_sampled_image(set, 1, 0, img);
		REQUIRE(update_sampled_res.is_ok());

		// Test Storage Image update (binding = 3)
		auto update_storage_img_res = device->uniform_set_update_storage_image(set, 3, 0, img);
		REQUIRE(update_storage_img_res.is_ok());

		// Test Storage Buffer update (binding = 4)
		auto buf_res = device->buffer_create(
				256, BUFFER_USAGE_STORAGE_BUFFER_BIT, MemoryAllocationType::GPU);
		REQUIRE(buf_res.is_ok());
		Buffer buf = buf_res.value();

		auto update_buf_res = device->uniform_set_update_buffer(set, 4, 0, buf);
		REQUIRE(update_buf_res.is_ok());

		// Free resources
		REQUIRE(device->buffer_free(buf).is_ok());
		REQUIRE(device->sampler_free(sampler).is_ok());
		REQUIRE(device->image_free(img).is_ok());
		REQUIRE(device->uniform_set_free(set).is_ok());
	}

	REQUIRE(device->shader_free(shader).is_ok());
}
