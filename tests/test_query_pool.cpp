#include <catch2/catch_test_macros.hpp>

#include "test_common.h"

using namespace gpukit;

TEST_CASE("Query Pool", "[query_pool]") {
	gpukit::test::ensure_test_device();

	SECTION("Create and free") {
		auto res = query_pool_create(4);
		REQUIRE(res.is_ok());
		QueryPool pool = res.value();
		REQUIRE(pool != GL_NULL_HANDLE);
		REQUIRE(query_pool_free(pool).is_ok());
	}

	SECTION("Single timestamp write") {
		auto pool_res = query_pool_create(1);
		REQUIRE(pool_res.is_ok());
		QueryPool qpool = pool_res.value();

		auto queue_res = queue_get(QueueType::GRAPHICS);
		REQUIRE(queue_res.is_ok());
		CommandQueue queue = queue_res.value();

		auto cmd_pool_res = command_pool_create(queue);
		REQUIRE(cmd_pool_res.is_ok());
		CommandPool cmd_pool = cmd_pool_res.value();

		auto cmd_res = command_pool_allocate(cmd_pool);
		REQUIRE(cmd_res.is_ok());
		CommandBuffer cmd = cmd_res.value();

		REQUIRE(command_begin(cmd).is_ok());
		REQUIRE(command_reset_query_pool(cmd, qpool, 0, 1).is_ok());
		REQUIRE(command_write_timestamp(cmd, qpool, 0,
					PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT).is_ok());
		REQUIRE(command_end(cmd).is_ok());

		Fence fence = fence_create(false);
		REQUIRE(queue_submit(queue, cmd, fence).is_ok());
		REQUIRE(fence_wait(fence).is_ok());

		auto results_res = query_pool_get_results(qpool, 0, 1);
		REQUIRE(results_res.is_ok());
		REQUIRE(results_res.value().size() == 1);

		REQUIRE(fence_free(fence).is_ok());
		REQUIRE(command_pool_free(cmd_pool).is_ok());
		REQUIRE(query_pool_free(qpool).is_ok());
	}

	SECTION("Two timestamps: end >= start") {
		auto pool_res = query_pool_create(2);
		REQUIRE(pool_res.is_ok());
		QueryPool qpool = pool_res.value();

		auto queue_res = queue_get(QueueType::GRAPHICS);
		REQUIRE(queue_res.is_ok());
		CommandQueue queue = queue_res.value();

		auto cmd_pool_res = command_pool_create(queue);
		REQUIRE(cmd_pool_res.is_ok());
		CommandPool cmd_pool = cmd_pool_res.value();

		auto cmd_res = command_pool_allocate(cmd_pool);
		REQUIRE(cmd_res.is_ok());
		CommandBuffer cmd = cmd_res.value();

		REQUIRE(command_begin(cmd).is_ok());
		REQUIRE(command_reset_query_pool(cmd, qpool, 0, 2).is_ok());
		REQUIRE(command_write_timestamp(cmd, qpool, 0,
					PIPELINE_STAGE_TOP_OF_PIPE_BIT).is_ok());
		REQUIRE(command_write_timestamp(cmd, qpool, 1,
					PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT).is_ok());
		REQUIRE(command_end(cmd).is_ok());

		Fence fence = fence_create(false);
		REQUIRE(queue_submit(queue, cmd, fence).is_ok());
		REQUIRE(fence_wait(fence).is_ok());

		auto results_res = query_pool_get_results(qpool, 0, 2);
		REQUIRE(results_res.is_ok());
		const auto& ts = results_res.value();
		REQUIRE(ts.size() == 2);
		REQUIRE(ts[1] >= ts[0]);

		REQUIRE(fence_free(fence).is_ok());
		REQUIRE(command_pool_free(cmd_pool).is_ok());
		REQUIRE(query_pool_free(qpool).is_ok());
	}

	SECTION("Timestamps around a buffer copy") {
		auto pool_res = query_pool_create(2);
		REQUIRE(pool_res.is_ok());
		QueryPool qpool = pool_res.value();

		constexpr uint64_t BUF_SIZE = 1024 * 1024; // 1 MiB
		auto src_res = buffer_create(BUF_SIZE, BUFFER_USAGE_TRANSFER_SRC_BIT, MemoryAllocationType::CPU);
		REQUIRE(src_res.is_ok());
		auto dst_res = buffer_create(BUF_SIZE, BUFFER_USAGE_TRANSFER_DST_BIT, MemoryAllocationType::CPU);
		REQUIRE(dst_res.is_ok());
		Buffer src_buf = src_res.value();
		Buffer dst_buf = dst_res.value();

		auto queue_res = queue_get(QueueType::GRAPHICS);
		REQUIRE(queue_res.is_ok());
		CommandQueue queue = queue_res.value();

		auto cmd_pool_res = command_pool_create(queue);
		REQUIRE(cmd_pool_res.is_ok());
		CommandPool cmd_pool = cmd_pool_res.value();

		auto cmd_res = command_pool_allocate(cmd_pool);
		REQUIRE(cmd_res.is_ok());
		CommandBuffer cmd = cmd_res.value();

		REQUIRE(command_begin(cmd).is_ok());
		REQUIRE(command_reset_query_pool(cmd, qpool, 0, 2).is_ok());
		REQUIRE(command_write_timestamp(cmd, qpool, 0, PIPELINE_STAGE_TRANSFER_BIT).is_ok());
		BufferCopyRegion region = { 0, 0, BUF_SIZE };
		REQUIRE(command_copy_buffer(cmd, src_buf, dst_buf, region).is_ok());
		REQUIRE(command_write_timestamp(cmd, qpool, 1, PIPELINE_STAGE_TRANSFER_BIT).is_ok());
		REQUIRE(command_end(cmd).is_ok());

		Fence fence = fence_create(false);
		REQUIRE(queue_submit(queue, cmd, fence).is_ok());
		REQUIRE(fence_wait(fence).is_ok());

		auto results_res = query_pool_get_results(qpool, 0, 2);
		REQUIRE(results_res.is_ok());
		const auto& ts = results_res.value();
		REQUIRE(ts.size() == 2);
		REQUIRE(ts[1] >= ts[0]);

		double elapsed_ns = timestamps_to_ns(ts[1] - ts[0]);
		REQUIRE(elapsed_ns >= 0.0);

		REQUIRE(fence_free(fence).is_ok());
		REQUIRE(command_pool_free(cmd_pool).is_ok());
		REQUIRE(buffer_free(src_buf).is_ok());
		REQUIRE(buffer_free(dst_buf).is_ok());
		REQUIRE(query_pool_free(qpool).is_ok());
	}

	SECTION("timestamps_to_ns returns non-negative value") {
		REQUIRE(timestamps_to_ns(0) == 0.0);
		REQUIRE(timestamps_to_ns(1000000) >= 0.0);
	}

}
