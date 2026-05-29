#include <catch2/catch_test_macros.hpp>

#include "gpukit/raii/raii.h"
#include "test_common.h"

namespace raii = gpukit::raii;
using namespace gpukit;

TEST_CASE("RAII — default-constructed handle is null", "[raii]") {
	raii::Fence f;
	CHECK(!f);
	CHECK(f == nullptr);
	CHECK(f.get() == GL_NULL_HANDLE);
}

TEST_CASE("RAII — move constructor", "[raii]") {
	gpukit::test::ensure_test_device();

	raii::Fence a = fence_create(false);
	REQUIRE(a);
	Fence raw = a.get();

	raii::Fence b = std::move(a);
	CHECK(!a);
	CHECK(b);
	CHECK(b.get() == raw);
}

TEST_CASE("RAII — move assignment transfers ownership and frees old resource", "[raii]") {
	gpukit::test::ensure_test_device();

	raii::Fence a = fence_create(false);
	raii::Fence b = fence_create(false);
	REQUIRE(a);
	REQUIRE(b);

	Fence raw_a = a.get();
	b = std::move(a);

	CHECK(!a);
	CHECK(b.get() == raw_a);
	// old b resource was freed by the assignment — no double-free on scope exit
}

TEST_CASE("RAII — self-move is safe", "[raii]") {
	gpukit::test::ensure_test_device();

	raii::Fence f = fence_create(false);
	REQUIRE(f);

	// Must not crash or corrupt state
	f = std::move(f);
	// No CHECK on validity — the standard leaves moved-from state unspecified,
	// we only care that it doesn't blow up.
}

TEST_CASE("RAII — adopt takes ownership of a raw handle", "[raii]") {
	gpukit::test::ensure_test_device();

	Fence raw = fence_create(false);
	REQUIRE(raw != GL_NULL_HANDLE);

	auto f = raii::Fence::adopt(raw);
	CHECK(f.get() == raw);
	CHECK(f);
	// f frees raw on scope exit
}

TEST_CASE("RAII — release relinquishes ownership", "[raii]") {
	gpukit::test::ensure_test_device();

	raii::Fence f = fence_create(false);
	REQUIRE(f);

	Fence raw = f.release();
	CHECK(!f);
	CHECK(raw != GL_NULL_HANDLE);

	REQUIRE(fence_free(raw).is_ok());
}

TEST_CASE("RAII — release on null handle returns GL_NULL_HANDLE", "[raii]") {
	raii::Fence f;
	Fence raw = f.release();
	CHECK(raw == GL_NULL_HANDLE);
	CHECK(!f);
}

TEST_CASE("RAII — reset() frees resource and nullifies", "[raii]") {
	gpukit::test::ensure_test_device();

	raii::Fence f = fence_create(false);
	REQUIRE(f);
	f.reset();
	CHECK(!f);
}

TEST_CASE("RAII — reset(h) replaces ownership", "[raii]") {
	gpukit::test::ensure_test_device();

	raii::Fence a = fence_create(false);
	Fence raw_b = fence_create(false);
	REQUIRE(a);
	REQUIRE(raw_b != GL_NULL_HANDLE);

	// Frees a's original resource, then takes ownership of raw_b
	a.reset(raw_b);
	CHECK(a.get() == raw_b);
}

TEST_CASE("RAII — free() surfaces Res<> and nullifies", "[raii]") {
	gpukit::test::ensure_test_device();

	SECTION("free on valid handle succeeds and nullifies") {
		raii::Fence f = fence_create(false);
		REQUIRE(f);
		auto res = f.free();
		CHECK(res.is_ok());
		CHECK(!f);
	}

	SECTION("free on null handle is a no-op returning ok") {
		raii::Fence f;
		CHECK(f.free().is_ok());
	}

	SECTION("second free after first is a no-op") {
		raii::Fence f = fence_create(false);
		REQUIRE(f.free().is_ok());
		CHECK(f.free().is_ok()); // null handle — no deleter called
	}
}

TEST_CASE("RAII — swap exchanges ownership", "[raii]") {
	gpukit::test::ensure_test_device();

	raii::Fence a = fence_create(false);
	raii::Fence b = fence_create(false);
	REQUIRE(a);
	REQUIRE(b);

	Fence raw_a = a.get();
	Fence raw_b = b.get();

	a.swap(b);

	CHECK(a.get() == raw_b);
	CHECK(b.get() == raw_a);
}

TEST_CASE("RAII — implicit conversion to raw handle", "[raii]") {
	gpukit::test::ensure_test_device();

	raii::Fence f = fence_create(false);
	REQUIRE(f);

	Fence raw = f; // operator H()
	CHECK(raw == f.get());
}

TEST_CASE("RAII — Device free() on null returns ok (void deleter path)", "[raii]") {
	// Verifies the _Caller<Ret, void> specialization compiles and behaves
	// correctly. We use a null Device to avoid interfering with the shared
	// test device managed by test_common.
	raii::Device d;
	auto res = d.free();
	CHECK(res.is_ok());
	CHECK(!d);
}

TEST_CASE("RAII — Buffer ownership through scope", "[raii]") {
	gpukit::test::ensure_test_device();

	SECTION("Buffer freed on scope exit") {
		raii::Buffer buf =
				buffer_create(64, BUFFER_USAGE_UNIFORM_BUFFER_BIT, MemoryAllocationType::CPU)
						.value();
		REQUIRE(buf);
		// buf freed here — no leak, no double-free
	}

	SECTION("Buffer moved out of a helper lambda") {
		auto make_buf = []() -> raii::Buffer {
			return buffer_create(64, BUFFER_USAGE_UNIFORM_BUFFER_BIT, MemoryAllocationType::CPU)
					.value();
		};

		raii::Buffer buf = make_buf();
		REQUIRE(buf);
		CHECK(buf.free().is_ok());
	}

	SECTION("Swapping two Buffers") {
		raii::Buffer a =
				buffer_create(64, BUFFER_USAGE_UNIFORM_BUFFER_BIT, MemoryAllocationType::CPU)
						.value();
		raii::Buffer b =
				buffer_create(128, BUFFER_USAGE_UNIFORM_BUFFER_BIT, MemoryAllocationType::CPU)
						.value();

		Buffer raw_a = a.get();
		Buffer raw_b = b.get();

		a.swap(b);
		CHECK(a.get() == raw_b);
		CHECK(b.get() == raw_a);
	}
}
