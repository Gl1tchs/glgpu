#include <catch2/catch_test_macros.hpp>

#include <cstring>

#include "test_common.h"

using namespace gpukit;

TEST_CASE("Buffer Operations", "[buffer]") {
	gpukit::test::ensure_test_device();

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

	SECTION("Upload and readback via buffer_upload") {
		constexpr uint64_t size = 64;
		uint8_t src[size];
		for (size_t i = 0; i < size; ++i)
			src[i] = static_cast<uint8_t>(i);

		auto buf_res = buffer_create(size, BUFFER_USAGE_TRANSFER_SRC_BIT, MemoryAllocationType::CPU);
		REQUIRE(buf_res.is_ok());
		Buffer buf = buf_res.value();

		REQUIRE(buffer_upload(buf, src, size).is_ok());

		auto map_res = buffer_map(buf);
		REQUIRE(map_res.is_ok());
		REQUIRE(memcmp(map_res.value(), src, size) == 0);
		REQUIRE(buffer_unmap(buf).is_ok());
		REQUIRE(buffer_free(buf).is_ok());
	}

	SECTION("Partial upload with offset") {
		constexpr uint64_t size = 128;
		constexpr uint64_t offset = 64;
		uint8_t src[64];
		memset(src, 0xBB, sizeof(src));

		auto buf_res = buffer_create(size, BUFFER_USAGE_TRANSFER_SRC_BIT, MemoryAllocationType::CPU);
		REQUIRE(buf_res.is_ok());
		Buffer buf = buf_res.value();

		REQUIRE(buffer_upload(buf, src, sizeof(src), offset).is_ok());

		auto map_res = buffer_map(buf);
		REQUIRE(map_res.is_ok());
		uint8_t* ptr = map_res.value();
		REQUIRE(memcmp(ptr + offset, src, sizeof(src)) == 0);
		REQUIRE(buffer_unmap(buf).is_ok());
		REQUIRE(buffer_free(buf).is_ok());
	}

	SECTION("Uniform buffer") {
		auto buf_res = buffer_create(256, BUFFER_USAGE_UNIFORM_BUFFER_BIT, MemoryAllocationType::CPU);
		REQUIRE(buf_res.is_ok());
		Buffer buf = buf_res.value();
		REQUIRE(buf != GL_NULL_HANDLE);
		REQUIRE(buffer_free(buf).is_ok());
	}

	SECTION("Vertex buffer") {
		auto buf_res = buffer_create(1024,
				BUFFER_USAGE_VERTEX_BUFFER_BIT | BUFFER_USAGE_TRANSFER_DST_BIT,
				MemoryAllocationType::GPU);
		REQUIRE(buf_res.is_ok());
		Buffer buf = buf_res.value();
		REQUIRE(buf != GL_NULL_HANDLE);
		REQUIRE(buffer_free(buf).is_ok());
	}

	SECTION("Index buffer") {
		auto buf_res = buffer_create(512,
				BUFFER_USAGE_INDEX_BUFFER_BIT | BUFFER_USAGE_TRANSFER_DST_BIT,
				MemoryAllocationType::GPU);
		REQUIRE(buf_res.is_ok());
		Buffer buf = buf_res.value();
		REQUIRE(buf != GL_NULL_HANDLE);
		REQUIRE(buffer_free(buf).is_ok());
	}

	SECTION("Indirect buffer") {
		auto buf_res = buffer_create(256,
				BUFFER_USAGE_INDIRECT_BUFFER_BIT | BUFFER_USAGE_TRANSFER_DST_BIT,
				MemoryAllocationType::GPU);
		REQUIRE(buf_res.is_ok());
		Buffer buf = buf_res.value();
		REQUIRE(buf != GL_NULL_HANDLE);
		REQUIRE(buffer_free(buf).is_ok());
	}

	SECTION("Flush and invalidate on CPU buffer") {
		auto buf_res = buffer_create(64, BUFFER_USAGE_UNIFORM_BUFFER_BIT, MemoryAllocationType::CPU);
		REQUIRE(buf_res.is_ok());
		Buffer buf = buf_res.value();

		auto map_res = buffer_map(buf);
		REQUIRE(map_res.is_ok());
		memset(map_res.value(), 0xCC, 64);

		REQUIRE(buffer_flush(buf).is_ok());
		REQUIRE(buffer_invalidate(buf).is_ok());
		REQUIRE(buffer_unmap(buf).is_ok());
		REQUIRE(buffer_free(buf).is_ok());
	}

}
