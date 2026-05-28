#include <catch2/catch_test_macros.hpp>

#include <cstring>

#include "test_common.h"

using namespace gpukit;

TEST_CASE("Command Queue & Submission", "[queue][command]") {
	gpukit::test::ensure_test_device();

	SECTION("Get graphics queue") {
		auto q = queue_get(QueueType::GRAPHICS);
		REQUIRE(q.is_ok());
		REQUIRE(q.value() != GL_NULL_HANDLE);
	}

	SECTION("Get transfer queue") {
		auto q = queue_get(QueueType::TRANSFER);
		REQUIRE(q.is_ok());
		REQUIRE(q.value() != GL_NULL_HANDLE);
	}

	SECTION("Get compute queue") {
		auto q = queue_get(QueueType::COMPUTE);
		REQUIRE(q.is_ok());
		REQUIRE(q.value() != GL_NULL_HANDLE);
	}

	SECTION("Command pool lifecycle") {
		auto queue_res = queue_get(QueueType::GRAPHICS);
		REQUIRE(queue_res.is_ok());
		CommandQueue queue = queue_res.value();

		auto pool_res = command_pool_create(queue);
		REQUIRE(pool_res.is_ok());
		CommandPool pool = pool_res.value();
		REQUIRE(pool != GL_NULL_HANDLE);

		REQUIRE(command_pool_free(pool).is_ok());
	}

	SECTION("Allocate and record a command buffer") {
		auto queue_res = queue_get(QueueType::GRAPHICS);
		REQUIRE(queue_res.is_ok());
		CommandQueue queue = queue_res.value();

		auto pool_res = command_pool_create(queue);
		REQUIRE(pool_res.is_ok());
		CommandPool pool = pool_res.value();

		auto cmd_res = command_pool_allocate(pool);
		REQUIRE(cmd_res.is_ok());
		CommandBuffer cmd = cmd_res.value();
		REQUIRE(cmd != GL_NULL_HANDLE);

		REQUIRE(command_begin(cmd).is_ok());
		REQUIRE(command_end(cmd).is_ok());

		REQUIRE(command_pool_free(pool).is_ok());
	}

	SECTION("Allocate multiple command buffers from one pool") {
		auto queue_res = queue_get(QueueType::GRAPHICS);
		REQUIRE(queue_res.is_ok());
		CommandQueue queue = queue_res.value();

		auto pool_res = command_pool_create(queue);
		REQUIRE(pool_res.is_ok());
		CommandPool pool = pool_res.value();

		auto cmds_res = command_pool_allocate(pool, 4);
		REQUIRE(cmds_res.is_ok());
		REQUIRE(cmds_res.value().size() == 4);
		for (CommandBuffer cmd : cmds_res.value())
			REQUIRE(cmd != GL_NULL_HANDLE);

		REQUIRE(command_pool_free(pool).is_ok());
	}

	SECTION("Command pool reset and re-record") {
		auto queue_res = queue_get(QueueType::GRAPHICS);
		REQUIRE(queue_res.is_ok());
		CommandQueue queue = queue_res.value();

		auto pool_res = command_pool_create(queue);
		REQUIRE(pool_res.is_ok());
		CommandPool pool = pool_res.value();

		auto cmd_res = command_pool_allocate(pool);
		REQUIRE(cmd_res.is_ok());
		CommandBuffer cmd = cmd_res.value();

		REQUIRE(command_begin(cmd).is_ok());
		REQUIRE(command_end(cmd).is_ok());

		REQUIRE(command_pool_reset(pool).is_ok());

		REQUIRE(command_begin(cmd).is_ok());
		REQUIRE(command_end(cmd).is_ok());

		REQUIRE(command_pool_free(pool).is_ok());
	}

	SECTION("Queue submit with fence and CPU wait") {
		auto queue_res = queue_get(QueueType::GRAPHICS);
		REQUIRE(queue_res.is_ok());
		CommandQueue queue = queue_res.value();

		auto pool_res = command_pool_create(queue);
		REQUIRE(pool_res.is_ok());
		CommandPool pool = pool_res.value();

		auto cmd_res = command_pool_allocate(pool);
		REQUIRE(cmd_res.is_ok());
		CommandBuffer cmd = cmd_res.value();

		REQUIRE(command_begin(cmd).is_ok());
		REQUIRE(command_end(cmd).is_ok());

		Fence fence = fence_create(false);
		REQUIRE(fence != GL_NULL_HANDLE);

		REQUIRE(queue_submit(queue, cmd, fence).is_ok());
		REQUIRE(fence_wait(fence).is_ok());

		REQUIRE(fence_free(fence).is_ok());
		REQUIRE(command_pool_free(pool).is_ok());
	}

	SECTION("Fence reset and reuse") {
		auto queue_res = queue_get(QueueType::GRAPHICS);
		REQUIRE(queue_res.is_ok());
		CommandQueue queue = queue_res.value();

		auto pool_res = command_pool_create(queue);
		REQUIRE(pool_res.is_ok());
		CommandPool pool = pool_res.value();

		Fence fence = fence_create(false);
		REQUIRE(fence != GL_NULL_HANDLE);

		for (int i = 0; i < 3; ++i) {
			auto cmd_res = command_pool_allocate(pool);
			REQUIRE(cmd_res.is_ok());
			CommandBuffer cmd = cmd_res.value();

			REQUIRE(command_begin(cmd).is_ok());
			REQUIRE(command_end(cmd).is_ok());

			REQUIRE(queue_submit(queue, cmd, fence).is_ok());
			REQUIRE(fence_wait(fence).is_ok());
			REQUIRE(fence_reset(fence).is_ok());
			REQUIRE(command_pool_reset(pool).is_ok());
		}

		REQUIRE(fence_free(fence).is_ok());
		REQUIRE(command_pool_free(pool).is_ok());
	}

	SECTION("Binary semaphore: GPU signal, GPU wait") {
		auto queue_res = queue_get(QueueType::GRAPHICS);
		REQUIRE(queue_res.is_ok());
		CommandQueue queue = queue_res.value();

		auto pool_res = command_pool_create(queue);
		REQUIRE(pool_res.is_ok());
		CommandPool pool = pool_res.value();

		auto cmds_res = command_pool_allocate(pool, 2);
		REQUIRE(cmds_res.is_ok());
		CommandBuffer cmd1 = cmds_res.value()[0];
		CommandBuffer cmd2 = cmds_res.value()[1];

		Semaphore sem = semaphore_create();
		REQUIRE(sem != GL_NULL_HANDLE);

		Fence fence = fence_create(false);

		REQUIRE(command_begin(cmd1).is_ok());
		REQUIRE(command_end(cmd1).is_ok());
		REQUIRE(queue_submit(queue, cmd1, GL_NULL_HANDLE, GL_NULL_HANDLE, sem).is_ok());

		REQUIRE(command_begin(cmd2).is_ok());
		REQUIRE(command_end(cmd2).is_ok());
		REQUIRE(queue_submit(queue, cmd2, fence, sem, GL_NULL_HANDLE).is_ok());
		REQUIRE(fence_wait(fence).is_ok());

		REQUIRE(semaphore_free(sem).is_ok());
		REQUIRE(fence_free(fence).is_ok());
		REQUIRE(command_pool_free(pool).is_ok());
	}

	SECTION("GPU buffer copy via command_copy_buffer") {
		constexpr uint64_t size = 128;
		uint8_t src_data[size];
		for (size_t i = 0; i < size; ++i)
			src_data[i] = static_cast<uint8_t>(i & 0xFF);

		auto src_res = buffer_create(size, BUFFER_USAGE_TRANSFER_SRC_BIT, MemoryAllocationType::CPU);
		REQUIRE(src_res.is_ok());
		Buffer src_buf = src_res.value();
		REQUIRE(buffer_upload(src_buf, src_data, size).is_ok());

		auto dst_res = buffer_create(size, BUFFER_USAGE_TRANSFER_DST_BIT, MemoryAllocationType::CPU);
		REQUIRE(dst_res.is_ok());
		Buffer dst_buf = dst_res.value();

		auto queue_res = queue_get(QueueType::TRANSFER);
		REQUIRE(queue_res.is_ok());
		CommandQueue queue = queue_res.value();

		auto pool_res = command_pool_create(queue);
		REQUIRE(pool_res.is_ok());
		CommandPool pool = pool_res.value();

		auto cmd_res = command_pool_allocate(pool);
		REQUIRE(cmd_res.is_ok());
		CommandBuffer cmd = cmd_res.value();

		REQUIRE(command_begin(cmd).is_ok());
		BufferCopyRegion region = { .src_offset = 0, .dst_offset = 0, .size = size };
		REQUIRE(command_copy_buffer(cmd, src_buf, dst_buf, region).is_ok());
		REQUIRE(command_end(cmd).is_ok());

		Fence fence = fence_create(false);
		REQUIRE(queue_submit(queue, cmd, fence).is_ok());
		REQUIRE(fence_wait(fence).is_ok());

		auto map_res = buffer_map(dst_buf);
		REQUIRE(map_res.is_ok());
		REQUIRE(buffer_invalidate(dst_buf).is_ok());
		REQUIRE(memcmp(map_res.value(), src_data, size) == 0);
		REQUIRE(buffer_unmap(dst_buf).is_ok());

		REQUIRE(fence_free(fence).is_ok());
		REQUIRE(command_pool_free(pool).is_ok());
		REQUIRE(buffer_free(src_buf).is_ok());
		REQUIRE(buffer_free(dst_buf).is_ok());
	}

	SECTION("Image layout transition via command_transition_image") {
		ImageCreateInfo img_info = {};
		img_info.size = { 64, 64 };
		img_info.format = DataFormat::R8G8B8A8_UNORM;
		img_info.usage = IMAGE_USAGE_SAMPLED_BIT | IMAGE_USAGE_TRANSFER_DST_BIT;
		img_info.samples = 1;

		auto img_res = image_create(img_info);
		REQUIRE(img_res.is_ok());
		Image img = img_res.value();

		auto queue_res = queue_get(QueueType::GRAPHICS);
		REQUIRE(queue_res.is_ok());
		CommandQueue queue = queue_res.value();

		auto pool_res = command_pool_create(queue);
		REQUIRE(pool_res.is_ok());
		CommandPool pool = pool_res.value();

		auto cmd_res = command_pool_allocate(pool);
		REQUIRE(cmd_res.is_ok());
		CommandBuffer cmd = cmd_res.value();

		REQUIRE(command_begin(cmd).is_ok());
		REQUIRE(command_transition_image(cmd, img,
					ImageLayout::UNDEFINED,
					ImageLayout::TRANSFER_DST_OPTIMAL).is_ok());
		REQUIRE(command_transition_image(cmd, img,
					ImageLayout::TRANSFER_DST_OPTIMAL,
					ImageLayout::SHADER_READ_ONLY_OPTIMAL).is_ok());
		REQUIRE(command_end(cmd).is_ok());

		Fence fence = fence_create(false);
		REQUIRE(queue_submit(queue, cmd, fence).is_ok());
		REQUIRE(fence_wait(fence).is_ok());

		REQUIRE(fence_free(fence).is_ok());
		REQUIRE(command_pool_free(pool).is_ok());
		REQUIRE(image_free(img).is_ok());
	}

	SECTION("Immediate submit") {
		bool was_called = false;
		auto res = command_immediate_submit([&was_called](CommandBuffer) {
			was_called = true;
		});
		REQUIRE(res.is_ok());
		REQUIRE(was_called);
	}

}
