#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include <vector>

#include "gpukit/compute/stream.h"
#include "test_common.h"

using namespace gpukit;

TEST_CASE("Stream sync with no dispatches is a no-op", "[compute][stream]") {
	gpukit::test::ensure_test_device();

	Stream s;
	REQUIRE(s.sync().is_ok());
	REQUIRE(s.sync().is_ok()); // idempotent
}

TEST_CASE("Stream single-buffer dispatch", "[compute][stream]") {
	gpukit::test::ensure_test_device();

	const uint32_t N = 256;
	Kernel k(gpukit::test::compute_asset("test_stream_double.comp").c_str());

	Tensor<float> t(N);
	std::vector<float> in(N);
	for (uint32_t i = 0; i < N; ++i)
		in[i] = static_cast<float>(i);
	REQUIRE(t.upload(in).is_ok());

	Stream s;
	REQUIRE(s.dispatch(k, N, t).is_ok());
	REQUIRE(s.sync().is_ok());

	std::vector<float> out(N, 0.0f);
	REQUIRE(t.download(out.data(), N).is_ok());
	for (uint32_t i = 0; i < N; ++i)
		REQUIRE(out[i] == Catch::Approx(in[i] * 2.0f));
}

TEST_CASE("Stream two-buffer dispatch", "[compute][stream]") {
	gpukit::test::ensure_test_device();

	const uint32_t N = 256;
	Kernel k(gpukit::test::compute_asset("test_stream_copy.comp").c_str());

	Tensor<float> src(N), dst(N);
	std::vector<float> in(N);
	for (uint32_t i = 0; i < N; ++i)
		in[i] = static_cast<float>(i) * 3.0f;
	REQUIRE(src.upload(in).is_ok());

	Stream s;
	REQUIRE(s.dispatch(k, N, src, dst).is_ok());
	REQUIRE(s.sync().is_ok());

	std::vector<float> out(N, 0.0f);
	REQUIRE(dst.download(out.data(), N).is_ok());
	for (uint32_t i = 0; i < N; ++i)
		REQUIRE(out[i] == Catch::Approx(in[i]));
}

TEST_CASE("Stream chained dispatches with auto-barrier", "[compute][stream]") {
	gpukit::test::ensure_test_device();

	const uint32_t N = 256;
	Kernel k(gpukit::test::compute_asset("test_stream_double.comp").c_str());

	// 1 → 2 → 4: two dispatches on the same buffer; Stream must insert a barrier.
	Tensor<float> t(N);
	REQUIRE(t.upload(std::vector<float>(N, 1.0f)).is_ok());

	Stream s;
	REQUIRE(s.dispatch(k, N, t).is_ok());
	REQUIRE(s.dispatch(k, N, t).is_ok());
	REQUIRE(s.sync().is_ok());

	std::vector<float> out(N, 0.0f);
	REQUIRE(t.download(out.data(), N).is_ok());
	for (uint32_t i = 0; i < N; ++i)
		REQUIRE(out[i] == Catch::Approx(4.0f));
}

TEST_CASE("Stream reuse after sync", "[compute][stream]") {
	gpukit::test::ensure_test_device();

	const uint32_t N = 64;
	Kernel k(gpukit::test::compute_asset("test_stream_double.comp").c_str());
	Tensor<float> t(N);
	REQUIRE(t.upload(std::vector<float>(N, 1.0f)).is_ok());

	Stream s;

	REQUIRE(s.dispatch(k, N, t).is_ok());
	REQUIRE(s.sync().is_ok()); // 1 → 2

	REQUIRE(s.dispatch(k, N, t).is_ok());
	REQUIRE(s.sync().is_ok()); // 2 → 4

	std::vector<float> out(N, 0.0f);
	REQUIRE(t.download(out.data(), N).is_ok());
	for (uint32_t i = 0; i < N; ++i)
		REQUIRE(out[i] == Catch::Approx(4.0f));
}
