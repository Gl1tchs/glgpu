#include <catch2/catch_test_macros.hpp>

#include "test_common.h"

using namespace gpukit;

TEST_CASE("Fence", "[fence][sync]") {
	gpukit::test::ensure_test_device();

	SECTION("Create and free") {
		Fence fence = fence_create();
		REQUIRE(fence != GL_NULL_HANDLE);
		REQUIRE(fence_free(fence).is_ok());
	}

	SECTION("Create signaled, wait returns immediately") {
		Fence fence = fence_create(true);
		REQUIRE(fence != GL_NULL_HANDLE);
		REQUIRE(fence_wait(fence).is_ok());
		REQUIRE(fence_free(fence).is_ok());
	}

	SECTION("Create unsignaled fence") {
		Fence fence = fence_create(false);
		REQUIRE(fence != GL_NULL_HANDLE);
		REQUIRE(fence_free(fence).is_ok());
	}

	SECTION("Reset signaled fence") {
		Fence fence = fence_create(true);
		REQUIRE(fence_wait(fence).is_ok());
		REQUIRE(fence_reset(fence).is_ok());
		REQUIRE(fence_free(fence).is_ok());
	}

	SECTION("GPU signals unsignaled fence") {
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

	SECTION("Fence reset and reuse across multiple submissions") {
		auto queue_res = queue_get(QueueType::GRAPHICS);
		REQUIRE(queue_res.is_ok());
		CommandQueue queue = queue_res.value();

		auto pool_res = command_pool_create(queue);
		REQUIRE(pool_res.is_ok());
		CommandPool pool = pool_res.value();

		Fence fence = fence_create(false);
		REQUIRE(fence != GL_NULL_HANDLE);

		for (int i = 0; i < 4; ++i) {
			REQUIRE(command_pool_reset(pool).is_ok());

			auto cmd_res = command_pool_allocate(pool);
			REQUIRE(cmd_res.is_ok());
			CommandBuffer cmd = cmd_res.value();

			REQUIRE(command_begin(cmd).is_ok());
			REQUIRE(command_end(cmd).is_ok());

			REQUIRE(queue_submit(queue, cmd, fence).is_ok());
			REQUIRE(fence_wait(fence).is_ok());
			REQUIRE(fence_reset(fence).is_ok());
		}

		REQUIRE(fence_free(fence).is_ok());
		REQUIRE(command_pool_free(pool).is_ok());
	}

}
