#include <catch2/catch_test_macros.hpp>

#include "test_common.h"

using namespace gl;

TEST_CASE("Debug Utils Operations", "[debug]") {
	auto device = gl::test::get_test_device();
	REQUIRE(device != nullptr);

	SECTION("Setting Debug Name on Buffer") {
		auto buf_res = device->buffer_create(
				256, BUFFER_USAGE_STORAGE_BUFFER_BIT, MemoryAllocationType::GPU);
		REQUIRE(buf_res.is_ok());
		Buffer buf = buf_res.value();

		// Set debug name - should return success (or no-op success if validation layer is off)
		auto name_res = device->set_debug_name(ObjectType::BUFFER, buf, "Test_GPU_Buffer");
		REQUIRE(name_res.is_ok());

		REQUIRE(device->buffer_free(buf).is_ok());
	}

	SECTION("Setting Debug Name on Image") {
		ImageCreateInfo img_info = {};
		img_info.size = { 128, 128 };
		img_info.format = DataFormat::R8G8B8A8_UNORM;
		img_info.usage = IMAGE_USAGE_SAMPLED_BIT;
		img_info.samples = 1;
		img_info.mipmapped = false;

		auto img_res = device->image_create(img_info);
		REQUIRE(img_res.is_ok());
		Image img = img_res.value();

		auto name_res = device->set_debug_name(ObjectType::IMAGE, img, "Test_GPU_Image");
		REQUIRE(name_res.is_ok());

		REQUIRE(device->image_free(img).is_ok());
	}

	SECTION("Command Debug Labels") {
		// Allocate a command buffer to test begin/end labels on command stream
		auto pool_res = device->command_pool_create(device->queue_get(QueueType::GRAPHICS).value());
		REQUIRE(pool_res.is_ok());
		CommandPool pool = pool_res.value();

		auto cmd_res = device->command_pool_allocate(pool);
		REQUIRE(cmd_res.is_ok());
		CommandBuffer cmd = cmd_res.value();

		REQUIRE(device->command_begin(cmd).is_ok());

		// Test label insertion
		auto begin_res = device->command_begin_label(cmd, "Simulation Step Pass", COLOR_GREEN);
		// Note: may return INVALID_OPERATION if debug utils extension is not loaded, which is
		// acceptable but should not crash.
		if (begin_res.is_ok()) {
			REQUIRE(device->command_end_label(cmd).is_ok());
		}

		REQUIRE(device->command_end(cmd).is_ok());
		REQUIRE(device->command_pool_free(pool).is_ok());
	}
}
