#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "gpukit/compute/tensor.h"
#include "test_common.h"

using namespace gpukit;

TEST_CASE("Tensor HOST memory", "[compute][tensor]") {
	gpukit::test::ensure_test_device();

	const uint64_t N = 256;
	Tensor<float> t(N, TensorMemory::HOST);

	REQUIRE(t.count() == N);
	REQUIRE(t.byte_size() == N * sizeof(float));
	REQUIRE(t.handle() != GL_NULL_HANDLE);
	REQUIRE(t.memory_type() == TensorMemory::HOST);

	SECTION("upload and download roundtrip") {
		std::vector<float> in(N), out(N, -1.0f);
		for (uint64_t i = 0; i < N; ++i)
			in[i] = static_cast<float>(i);

		REQUIRE(t.upload(in).is_ok());
		REQUIRE(t.download(out.data(), N).is_ok());

		for (uint64_t i = 0; i < N; ++i)
			REQUIRE(out[i] == in[i]);
	}

	SECTION("partial upload respects size") {
		std::vector<float> half(N / 2, 7.0f);
		REQUIRE(t.upload(half).is_ok());

		std::vector<float> out(N / 2, 0.0f);
		REQUIRE(t.download(out.data(), N / 2).is_ok());

		for (uint64_t i = 0; i < N / 2; ++i)
			REQUIRE(out[i] == 7.0f);
	}
}

TEST_CASE("Tensor DEVICE memory", "[compute][tensor]") {
	gpukit::test::ensure_test_device();

	const uint64_t N = 256;
	Tensor<float> t(N, TensorMemory::DEVICE);

	REQUIRE(t.count() == N);
	REQUIRE(t.byte_size() == N * sizeof(float));
	REQUIRE(t.handle() != GL_NULL_HANDLE);
	REQUIRE(t.memory_type() == TensorMemory::DEVICE);

	SECTION("upload and download via staging") {
		std::vector<float> in(N), out(N, -1.0f);
		for (uint64_t i = 0; i < N; ++i)
			in[i] = static_cast<float>(i) * 2.0f;

		REQUIRE(t.upload(in).is_ok());
		REQUIRE(t.download(out.data(), N).is_ok());

		for (uint64_t i = 0; i < N; ++i)
			REQUIRE(out[i] == in[i]);
	}
}

TEST_CASE("Tensor move semantics", "[compute][tensor]") {
	gpukit::test::ensure_test_device();

	Tensor<float> a(128, TensorMemory::HOST);
	Buffer original_handle = a.handle();
	REQUIRE(original_handle != GL_NULL_HANDLE);

	Tensor<float> b = std::move(a);
	REQUIRE(b.handle() == original_handle);
	REQUIRE(a.handle() == GL_NULL_HANDLE);
	REQUIRE(b.count() == 128);
}
