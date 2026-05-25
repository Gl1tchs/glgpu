#include <catch2/catch_test_macros.hpp>

#include "test_common.h"

using namespace gpukit;

TEST_CASE("Buffer Operations", "[buffer]") {
	gpukit::test::get_test_device();

	SECTION("CPU Buffer Creation & Mapping") {
		uint64_t size = 1024;
		auto buf_res = buffer_create(size,
				BUFFER_USAGE_TRANSFER_SRC_BIT | BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT,
				MemoryAllocationType::CPU);

		REQUIRE(buf_res.is_ok());
		Buffer buf = buf_res.value();
		REQUIRE(buf != GL_NULL_HANDLE);

		SECTION("Map and Write") {
			auto map_res = buffer_map(buf);
			REQUIRE(map_res.is_ok());
			uint8_t* ptr = map_res.value();
			REQUIRE(ptr != nullptr);

			memset(ptr, 0xAA, size);

			REQUIRE(buffer_flush(buf).is_ok());
			REQUIRE(buffer_unmap(buf).is_ok());
		}

		SECTION("Get Device Address") {
			auto addr_res = buffer_get_device_address(buf);
			if (addr_res.is_ok()) {
				REQUIRE(addr_res.value() != 0);
			}
		}

		REQUIRE(buffer_free(buf).is_ok());
	}

	SECTION("GPU Buffer Creation") {
		auto buf_res = buffer_create(256, BUFFER_USAGE_STORAGE_BUFFER_BIT, MemoryAllocationType::GPU);
		REQUIRE(buf_res.is_ok());
		Buffer buf = buf_res.value();
		REQUIRE(buffer_free(buf).is_ok());
	}
}
