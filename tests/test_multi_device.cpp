#include <catch2/catch_test_macros.hpp>

#include "gpukit/device.h"
#include "test_common.h"

using namespace gpukit;

TEST_CASE("Multi-Device", "[device][multi]") {
	gpukit::test::ensure_test_device();

	SECTION("Second device initializes and auto-selects") {
		auto res = init({});
		REQUIRE(res.is_ok());
		DeviceHandle dev2 = res.value();
		REQUIRE(dev2 != GL_NULL_HANDLE);
		REQUIRE(dev2 != gpukit::test::g_test_device);

		device_wait();
		shutdown(dev2);
		select_device(gpukit::test::g_test_device);
	}

	SECTION("Resources created on selected device") {
		auto res = init({});
		REQUIRE(res.is_ok());
		DeviceHandle dev2 = res.value();

		// dev2 is auto-selected — buffer goes to dev2
		auto buf = buffer_create(256, BUFFER_USAGE_STORAGE_BUFFER_BIT, MemoryAllocationType::CPU);
		REQUIRE(buf.is_ok());
		REQUIRE(buffer_free(buf.value()).is_ok());

		device_wait();
		shutdown(dev2);
		select_device(gpukit::test::g_test_device);
	}

	SECTION("select_device switches active context") {
		auto res = init({});
		REQUIRE(res.is_ok());
		DeviceHandle dev2 = res.value();

		// Switch to primary, allocate there
		select_device(gpukit::test::g_test_device);
		auto buf1 = buffer_create(128, BUFFER_USAGE_STORAGE_BUFFER_BIT, MemoryAllocationType::CPU);
		REQUIRE(buf1.is_ok());

		// Switch to dev2, allocate there
		select_device(dev2);
		auto buf2 = buffer_create(128, BUFFER_USAGE_STORAGE_BUFFER_BIT, MemoryAllocationType::CPU);
		REQUIRE(buf2.is_ok());

		// Free buf2 on dev2
		REQUIRE(buffer_free(buf2.value()).is_ok());
		device_wait();
		shutdown(dev2);

		// Back to primary, free buf1
		select_device(gpukit::test::g_test_device);
		REQUIRE(buffer_free(buf1.value()).is_ok());
	}

	SECTION("shutdown clears current device") {
		auto res = init({});
		REQUIRE(res.is_ok());
		DeviceHandle dev2 = res.value();

		// dev2 is current — shut it down
		device_wait();
		shutdown(dev2);

		// Restore primary for the rest of the suite
		select_device(gpukit::test::g_test_device);
		// Primary should still work fine
		auto buf = buffer_create(64, BUFFER_USAGE_STORAGE_BUFFER_BIT, MemoryAllocationType::CPU);
		REQUIRE(buf.is_ok());
		REQUIRE(buffer_free(buf.value()).is_ok());
	}
}
