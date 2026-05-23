#include <catch2/catch_test_macros.hpp>

#include "test_common.h"

#include <vector>

using namespace gl;

TEST_CASE("Bindless Uniform Management", "[uniform]") {
	auto device = gl::test::get_test_device();
	REQUIRE(device != nullptr);

	// Load bindless shaders
	auto shader_res = device->shader_create(
			"tests/assets/test_bindless.vert", "tests/assets/test_bindless.frag");
	if (shader_res.is_error()) {
		// Try fallback paths for ctest
		shader_res = device->shader_create(
				"../../tests/assets/test_bindless.vert", "../../tests/assets/test_bindless.frag");
	}
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

TEST_CASE("Automatic Shader Reflection & Empty Uniform Sets", "[uniform][reflection]") {
	auto device = gl::test::get_test_device();
	REQUIRE(device != nullptr);

	// Load bindless shaders
	auto shader_res = device->shader_create(
			"tests/assets/test_bindless.vert", "tests/assets/test_bindless.frag");
	if (shader_res.is_error()) {
		// Try fallback paths for ctest
		shader_res = device->shader_create(
				"../../tests/assets/test_bindless.vert", "../../tests/assets/test_bindless.frag");
	}
	REQUIRE(shader_res.is_ok());

	Shader shader = shader_res.value();

	SECTION("Shader Reflection Query") {
		auto resources_res = device->shader_get_resources(shader);
		REQUIRE(resources_res.is_ok());
		auto resources = resources_res.value();

		// Verify we reflected bindings from test_bindless shaders
		bool found_textures = false;
		bool found_sampled_images = false;
		bool found_samplers = false;
		bool found_storage_images = false;
		bool found_storage_buffers = false;

		for (const auto& res : resources) {
			if (res.set == 0) {
				if (res.binding == 0) {
					REQUIRE(res.type == ShaderUniformType::SAMPLER_WITH_TEXTURE);
					found_textures = true;
				} else if (res.binding == 1) {
					REQUIRE(res.type == ShaderUniformType::TEXTURE);
					found_sampled_images = true;
				} else if (res.binding == 2) {
					REQUIRE(res.type == ShaderUniformType::SAMPLER);
					found_samplers = true;
				} else if (res.binding == 3) {
					REQUIRE(res.type == ShaderUniformType::IMAGE);
					found_storage_images = true;
				} else if (res.binding == 4) {
					REQUIRE(res.type == ShaderUniformType::STORAGE_BUFFER);
					found_storage_buffers = true;
				}
			}
		}

		REQUIRE(found_textures);
		REQUIRE(found_sampled_images);
		REQUIRE(found_samplers);
		REQUIRE(found_storage_images);
		REQUIRE(found_storage_buffers);
	}

	SECTION("Create Empty Set and Update") {
		// Create empty set using reflected layout
		auto set_res = device->uniform_set_create(shader, 0);
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

		// Test Combined Image Sampler update
		auto update_tex_res = device->uniform_set_update_texture(set, 0, 0, img, sampler);
		REQUIRE(update_tex_res.is_ok());

		// Test Sampled Image update
		auto update_sampled_res = device->uniform_set_update_sampled_image(set, 1, 0, img);
		REQUIRE(update_sampled_res.is_ok());

		// Test Storage Image update
		auto update_storage_img_res = device->uniform_set_update_storage_image(set, 3, 0, img);
		REQUIRE(update_storage_img_res.is_ok());

		// Test Storage Buffer update
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
