#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "glgpu/math.h"

using namespace gl;

// Helper macro for verifying floating point vectors with a margin
// Using Catch2's WithinRel for relative comparison is robust for pow()
#define REQUIRE_VEC_CLOSE(v_actual, v_expected)                                                    \
	static_assert(v_actual.size() == v_expected.size());                                           \
	for (size_t i = 0; i < v_actual.size(); i++) {                                                 \
		REQUIRE_THAT(v_actual[i], Catch::Matchers::WithinRel(v_expected[i], 0.001f));              \
	}

TEST_CASE("Math Utilities", "[Math]") {
	SECTION("Degrees to Radians") {
		float deg = 180.0f;
		float rad = as_radians(deg);
		REQUIRE_THAT(rad, Catch::Matchers::WithinRel(3.14159265f, 0.0001f));

		REQUIRE(as_radians(0.0f) == 0.0f);

		float deg90 = 90.0f;
		REQUIRE_THAT(as_radians(deg90), Catch::Matchers::WithinRel(1.570796f, 0.0001f));
	}
}

TEST_CASE("Vector Power Functions", "[Math][Pow]") {
	SECTION("Vec2 Power") {
		Vec2f v(2.0f, 3.0f);
		// 2^2 = 4, 3^2 = 9
		Vec2f res = pow(v, 2.0f);
		REQUIRE_VEC_CLOSE(res, Vec2f(4.0f, 9.0f));
	}

	SECTION("Vec3 Power") {
		Vec3f v(2.0f, 4.0f, 10.0f);
		// 2^3 = 8, 4^3 = 64, 10^3 = 1000
		Vec3f res = pow(v, 3.0f);
		REQUIRE_VEC_CLOSE(res, Vec3f(8.0f, 64.0f, 1000.0f));
	}

	SECTION("Vec4 Power") {
		Vec4f v(1.0f, 2.0f, 3.0f, 4.0f);
		// x^0 = 1 for all
		Vec4f res = pow(v, 0.0f);
		REQUIRE_VEC_CLOSE(res, Vec4f(1.0f, 1.0f, 1.0f, 1.0f));
	}
}

TEST_CASE("Vector Min Functions", "[Math][Min]") {
	SECTION("Vec2 Min") {
		Vec2f a(1.0f, 10.0f);
		Vec2f b(5.0f, 5.0f);

		// Component-wise: min(1, 5)=1, min(10, 5)=5
		REQUIRE(min(a, b) == Vec2f(1.0f, 5.0f));

		// Scalar: min(1, 4)=1, min(10, 4)=4
		REQUIRE(min(a, 4.0f) == Vec2f(1.0f, 4.0f));
	}

	SECTION("Vec3 Min") {
		Vec3f a(10.0f, 20.0f, 30.0f);
		Vec3f b(5.0f, 25.0f, 15.0f);

		REQUIRE(min(a, b) == Vec3f(5.0f, 20.0f, 15.0f));
		REQUIRE(min(a, 15.0f) == Vec3f(10.0f, 15.0f, 15.0f));
	}

	SECTION("Vec4 Min") {
		Vec4f a(1.0f, 2.0f, 3.0f, 4.0f);
		Vec4f b(4.0f, 3.0f, 2.0f, 1.0f);

		REQUIRE(min(a, b) == Vec4f(1.0f, 2.0f, 2.0f, 1.0f));
		REQUIRE(min(a, 2.5f) == Vec4f(1.0f, 2.0f, 2.5f, 2.5f));
	}
}

TEST_CASE("Vector Max Functions", "[Math][Max]") {
	SECTION("Vec2 Max") {
		Vec2f a(1.0f, 10.0f);
		Vec2f b(5.0f, 5.0f);

		// Component-wise: max(1, 5)=5, max(10, 5)=10
		REQUIRE(max(a, b) == Vec2f(5.0f, 10.0f));

		// Scalar: max(1, 4)=4, max(10, 4)=10
		REQUIRE(max(a, 4.0f) == Vec2f(4.0f, 10.0f));
	}

	SECTION("Vec3 Max") {
		Vec3f a(10.0f, 20.0f, 30.0f);
		Vec3f b(5.0f, 25.0f, 15.0f);

		REQUIRE(max(a, b) == Vec3f(10.0f, 25.0f, 30.0f));
		REQUIRE(max(a, 15.0f) == Vec3f(15.0f, 20.0f, 30.0f));
	}

	SECTION("Vec4 Max") {
		Vec4f a(1.0f, 2.0f, 3.0f, 4.0f);
		Vec4f b(4.0f, 3.0f, 2.0f, 1.0f);

		REQUIRE(max(a, b) == Vec4f(4.0f, 3.0f, 3.0f, 4.0f));
		REQUIRE(max(a, 2.5f) == Vec4f(2.5f, 2.5f, 3.0f, 4.0f));
	}
}

TEST_CASE("Clamping Example", "[Math][Integration]") {
	Vec3f v(-5.0f, 10.0f, 50.0f);
	float lower = 0.0f;
	float upper = 1.0f;

	Vec3f clamped = clamp(v, lower, upper);

	REQUIRE(clamped.x == 0.0f); // -5 clamped to 0
	REQUIRE(clamped.y == 1.0f); // 10 clamped to 1
	REQUIRE(clamped.z == 1.0f); // 50 clamped to 1
}
