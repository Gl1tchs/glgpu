#include <catch2/catch_test_macros.hpp>

#include "test_common.h"

using namespace gpukit;

TEST_CASE("Debug Utils Operations", "[debug]") {
	auto device = gpukit::test::get_test_device();
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

	SECTION("Pipeline Barriers") {
		auto buf_res = device->buffer_create(
				256, BUFFER_USAGE_STORAGE_BUFFER_BIT, MemoryAllocationType::GPU);
		REQUIRE(buf_res.is_ok());
		Buffer buf = buf_res.value();

		ImageCreateInfo img_info = {};
		img_info.size = { 64, 64 };
		img_info.format = DataFormat::R8G8B8A8_UNORM;
		img_info.usage = IMAGE_USAGE_STORAGE_BIT | IMAGE_USAGE_SAMPLED_BIT;
		img_info.samples = 1;
		img_info.mipmapped = false;

		auto img_res = device->image_create(img_info);
		REQUIRE(img_res.is_ok());
		Image img = img_res.value();

		auto pool_res = device->command_pool_create(device->queue_get(QueueType::GRAPHICS).value());
		REQUIRE(pool_res.is_ok());
		CommandPool pool = pool_res.value();

		auto cmd_res = device->command_pool_allocate(pool);
		REQUIRE(cmd_res.is_ok());
		CommandBuffer cmd = cmd_res.value();

		REQUIRE(device->command_begin(cmd).is_ok());

		// Define a buffer barrier (e.g., from compute shader write to vertex input read / index
		// read)
		BufferBarrier buf_barrier = {};
		buf_barrier.buffer = buf;
		buf_barrier.src_stage = PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		buf_barrier.dst_stage = PIPELINE_STAGE_VERTEX_INPUT_BIT;
		buf_barrier.src_access = ACCESS_SHADER_WRITE_BIT;
		buf_barrier.dst_access = ACCESS_VERTEX_ATTRIBUTE_READ_BIT | ACCESS_INDEX_READ_BIT;
		buf_barrier.offset = 0;
		buf_barrier.size = 256;

		// Define an image barrier (e.g., from compute shader write to shader read)
		ImageBarrier img_barrier = {};
		img_barrier.image = img;
		img_barrier.src_stage = PIPELINE_STAGE_COMPUTE_SHADER_BIT;
		img_barrier.dst_stage = PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
		img_barrier.src_access = ACCESS_SHADER_WRITE_BIT;
		img_barrier.dst_access = ACCESS_SHADER_READ_BIT;
		img_barrier.old_layout = ImageLayout::GENERAL;
		img_barrier.new_layout = ImageLayout::SHADER_READ_ONLY_OPTIMAL;

		// Dispatch pipeline barrier
		std::vector<BufferBarrier> buf_barriers = { buf_barrier };
		std::vector<ImageBarrier> img_barriers = { img_barrier };
		auto barrier_res = device->command_pipeline_barrier(cmd, buf_barriers, img_barriers);
		REQUIRE(barrier_res.is_ok());

		REQUIRE(device->command_end(cmd).is_ok());
		REQUIRE(device->command_pool_free(pool).is_ok());
		REQUIRE(device->buffer_free(buf).is_ok());
		REQUIRE(device->image_free(img).is_ok());
	}
}
