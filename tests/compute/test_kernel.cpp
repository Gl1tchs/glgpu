#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "gpukit/compute/kernel.h"
#include "gpukit/compute/stream.h"
#include "gpukit/compute/tensor.h"
#include "test_common.h"

using namespace gpukit;

TEST_CASE("Kernel construction", "[compute][kernel]") {
	gpukit::test::ensure_test_device();

	SECTION("loads single-buffer compute shader") {
		Kernel k(gpukit::test::compute_asset("test_stream_double.comp").c_str());
		REQUIRE(k.shader() != GL_NULL_HANDLE);
		REQUIRE(k.pipeline() != GL_NULL_HANDLE);
	}

	SECTION("loads two-buffer compute shader") {
		Kernel k(gpukit::test::compute_asset("test_stream_copy.comp").c_str());
		REQUIRE(k.shader() != GL_NULL_HANDLE);
		REQUIRE(k.pipeline() != GL_NULL_HANDLE);
	}
}

TEST_CASE("Kernel move semantics", "[compute][kernel]") {
	gpukit::test::ensure_test_device();

	Kernel a(gpukit::test::compute_asset("test_stream_double.comp").c_str());
	Shader orig_shader = a.shader();
	Pipeline orig_pipeline = a.pipeline();

	Kernel b = std::move(a);
	REQUIRE(b.shader() == orig_shader);
	REQUIRE(b.pipeline() == orig_pipeline);
	REQUIRE(a.shader() == GL_NULL_HANDLE); // NOLINT(bugprone-use-after-move)
	REQUIRE(a.pipeline() == GL_NULL_HANDLE); // NOLINT(bugprone-use-after-move)

	// move-assign overwrites an existing valid kernel
	Kernel c(gpukit::test::compute_asset("test_stream_copy.comp").c_str());
	c = std::move(b);
	REQUIRE(c.shader() == orig_shader);
	REQUIRE(b.shader() == GL_NULL_HANDLE); // NOLINT(bugprone-use-after-move)
}

// -------------------------------------------------------------------------
// Kernel::from_source (JIT)
// -------------------------------------------------------------------------

static constexpr const char* k_double_source = R"glsl(
#version 450
layout(local_size_x = 64, local_size_y = 1, local_size_z = 1) in;
layout(set = 0, binding = 0, std430) buffer Buf_0 { float data[]; } buf;
void main() {
    uint i = gl_GlobalInvocationID.x;
    buf.data[i] = buf.data[i] * 2.0;
}
)glsl";

TEST_CASE("Kernel::from_source compiles and dispatches", "[compute][kernel][jit]") {
	gpukit::test::ensure_test_device();

	const uint32_t N = 128;
	Kernel k = Kernel::from_source(k_double_source);
	REQUIRE(k.shader() != GL_NULL_HANDLE);
	REQUIRE(k.pipeline() != GL_NULL_HANDLE);

	Tensor<float> t(N);
	std::vector<float> in(N);
	for (uint32_t i = 0; i < N; ++i)
		in[i] = static_cast<float>(i);
	REQUIRE(t.upload(in).is_ok());

	Stream s;
	REQUIRE(s.dispatch(k, t).is_ok());
	REQUIRE(s.sync().is_ok());

	std::vector<float> out(N, 0.0f);
	REQUIRE(t.download(out.data(), N).is_ok());
	for (uint32_t i = 0; i < N; ++i)
		REQUIRE(out[i] == Catch::Approx(in[i] * 2.0f));
}
