#include <catch2/catch_test_macros.hpp>

#include "gpukit/compute/kernel.h"
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
