#include <catch2/catch_test_macros.hpp>

#include "test_common.h"

using namespace gpukit;

TEST_CASE("Timeline Semaphore", "[sync][timeline_semaphore]") {
	gpukit::test::get_test_device();

	SECTION("Create and free") {
		auto sem_res = timeline_semaphore_create(0);
		REQUIRE(sem_res.is_ok());
		Semaphore sem = sem_res.value();
		REQUIRE(sem != GL_NULL_HANDLE);
		REQUIRE(semaphore_free(sem).is_ok());
	}

	SECTION("Initial value zero") {
		auto sem_res = timeline_semaphore_create(0);
		REQUIRE(sem_res.is_ok());
		Semaphore sem = sem_res.value();

		auto val_res = semaphore_get_value(sem);
		REQUIRE(val_res.is_ok());
		REQUIRE(val_res.value() == 0);

		REQUIRE(semaphore_free(sem).is_ok());
	}

	SECTION("Initial value non-zero") {
		auto sem_res = timeline_semaphore_create(42);
		REQUIRE(sem_res.is_ok());
		Semaphore sem = sem_res.value();

		auto val_res = semaphore_get_value(sem);
		REQUIRE(val_res.is_ok());
		REQUIRE(val_res.value() == 42);

		REQUIRE(semaphore_free(sem).is_ok());
	}

	SECTION("CPU signal advances counter") {
		auto sem_res = timeline_semaphore_create(0);
		REQUIRE(sem_res.is_ok());
		Semaphore sem = sem_res.value();

		REQUIRE(semaphore_signal(sem, 5).is_ok());

		auto val_res = semaphore_get_value(sem);
		REQUIRE(val_res.is_ok());
		REQUIRE(val_res.value() == 5);

		REQUIRE(semaphore_free(sem).is_ok());
	}

	SECTION("CPU wait on already-reached value returns immediately") {
		// initial_value = 10, waiting for 10 should not block
		auto sem_res = timeline_semaphore_create(10);
		REQUIRE(sem_res.is_ok());
		Semaphore sem = sem_res.value();

		REQUIRE(semaphore_wait(sem, 10).is_ok());

		REQUIRE(semaphore_free(sem).is_ok());
	}

	SECTION("CPU signal then CPU wait") {
		auto sem_res = timeline_semaphore_create(0);
		REQUIRE(sem_res.is_ok());
		Semaphore sem = sem_res.value();

		REQUIRE(semaphore_signal(sem, 1).is_ok());
		REQUIRE(semaphore_wait(sem, 1).is_ok());

		auto val_res = semaphore_get_value(sem);
		REQUIRE(val_res.is_ok());
		REQUIRE(val_res.value() >= 1);

		REQUIRE(semaphore_free(sem).is_ok());
	}

	SECTION("GPU signal, CPU wait") {
		auto sem_res = timeline_semaphore_create(0);
		REQUIRE(sem_res.is_ok());
		Semaphore sem = sem_res.value();

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

		REQUIRE(queue_submit(queue, cmd,
					SemaphoreSubmitInfo{},
					SemaphoreSubmitInfo{ .semaphore = sem, .value = 1 })
				.is_ok());

		REQUIRE(semaphore_wait(sem, 1).is_ok());

		auto val_res = semaphore_get_value(sem);
		REQUIRE(val_res.is_ok());
		REQUIRE(val_res.value() >= 1);

		REQUIRE(command_pool_free(pool).is_ok());
		REQUIRE(semaphore_free(sem).is_ok());
	}

	SECTION("CPU signal, GPU wait and signal, CPU wait") {
		// Models the frame-overlap pattern: CPU unblocks GPU, GPU signals completion.
		auto sem_res = timeline_semaphore_create(0);
		REQUIRE(sem_res.is_ok());
		Semaphore sem = sem_res.value();

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

		// CPU signals 1 so the GPU submission can proceed past its wait.
		REQUIRE(semaphore_signal(sem, 1).is_ok());

		// GPU waits for 1, then signals 2.
		REQUIRE(queue_submit(queue, cmd,
					SemaphoreSubmitInfo{ .semaphore = sem, .value = 1 },
					SemaphoreSubmitInfo{ .semaphore = sem, .value = 2 })
				.is_ok());

		// CPU waits for GPU to finish (value 2).
		REQUIRE(semaphore_wait(sem, 2).is_ok());

		auto val_res = semaphore_get_value(sem);
		REQUIRE(val_res.is_ok());
		REQUIRE(val_res.value() >= 2);

		REQUIRE(command_pool_free(pool).is_ok());
		REQUIRE(semaphore_free(sem).is_ok());
	}

	SECTION("Null handle operations return errors") {
		REQUIRE(semaphore_signal(GL_NULL_HANDLE, 1).is_error());
		REQUIRE(semaphore_wait(GL_NULL_HANDLE, 1, 0).is_error());
		REQUIRE(semaphore_get_value(GL_NULL_HANDLE).is_error());
	}
}
