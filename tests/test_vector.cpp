#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>

#include "glgpu/vector.h"

using namespace gl;

// Helper for floating point comparison with margin
bool close_enough(float a, float b, float epsilon = 1e-5f) { return std::abs(a - b) < epsilon; }

TEST_CASE("Vec2 Basic Operations", "[Vec2]") {
	SECTION("Construction") {
		Vec2f v1; // Default
		REQUIRE(v1.x == 0.0f);
		REQUIRE(v1.y == 0.0f);

		Vec2f v2(5.0f); // Scalar
		REQUIRE(v2.x == 5.0f);
		REQUIRE(v2.y == 5.0f);

		Vec2f v3(1.0f, 2.0f); // Components
		REQUIRE(v3.x == 1.0f);
		REQUIRE(v3.y == 2.0f);
	}

	SECTION("Static Constants") {
		REQUIRE(Vec2f::zero() == Vec2f(0.0f, 0.0f));
		REQUIRE(Vec2f::one() == Vec2f(1.0f, 1.0f));
		REQUIRE(Vec2f::right() == Vec2f(1.0f, 0.0f));
		REQUIRE(Vec2f::up() == Vec2f(0.0f, 1.0f));
	}

	SECTION("Arithmetic Operators") {
		Vec2f a(1.0f, 2.0f);
		Vec2f b(3.0f, 4.0f);

		REQUIRE((a + b) == Vec2f(4.0f, 6.0f));
		REQUIRE((b - a) == Vec2f(2.0f, 2.0f));
		REQUIRE((-a) == Vec2f(-1.0f, -2.0f));

		// Scalar math
		REQUIRE((a * 2.0f) == Vec2f(2.0f, 4.0f));
		REQUIRE((b / 2.0f) == Vec2f(1.5f, 2.0f));
	}

	SECTION("Geometric Functions") {
		Vec2f v(3.0f, 4.0f);

		// Dot product
		// (3*3) + (4*4) = 9 + 16 = 25
		REQUIRE(v.length_sq() == 25.0f);
		REQUIRE(v.length() == 5.0f);

		Vec2f norm = v.normalize();
		REQUIRE(close_enough(norm.x, 0.6f));
		REQUIRE(close_enough(norm.y, 0.8f));
		REQUIRE(close_enough(norm.length(), 1.0f));
	}
}

TEST_CASE("Vec3 Basic Operations", "[Vec3]") {
	SECTION("Construction & Conversion") {
		Vec3f v(1.0f, 2.0f, 3.0f);

		// Construct from Vec2
		Vec2f v2(10.0f, 20.0f);
		Vec3f v_from_2(v2, 5.0f);
		REQUIRE(v_from_2.x == 10.0f);
		REQUIRE(v_from_2.y == 20.0f);
		REQUIRE(v_from_2.z == 5.0f);
	}

	SECTION("Cross Product") {
		Vec3f right = Vec3f::right(); // (1, 0, 0)
		Vec3f up = Vec3f::up(); // (0, 1, 0)

		// Right x Up = Forward (0, 0, 1) assuming RH Y-up
		Vec3f res = right.cross(up);

		REQUIRE(res.x == 0.0f);
		REQUIRE(res.y == 0.0f);
		REQUIRE(res.z == 1.0f);

		// Order matters
		Vec3f res_inv = up.cross(right);
		REQUIRE(res_inv.z == -1.0f);
	}

	SECTION("Dot Product") {
		Vec3f a(1.0f, 0.0f, 0.0f);
		Vec3f b(0.5f, 1.0f, 0.0f);

		// Project b onto a
		REQUIRE(a.dot(b) == 0.5f);

		// Perpendicular
		REQUIRE(Vec3f::up().dot(Vec3f::right()) == 0.0f);
	}
}

TEST_CASE("Vec4 Basic Operations", "[Vec4]") {
	SECTION("Construction") {
		Vec3f v3(1.0f, 2.0f, 3.0f);
		Vec4f v4(v3, 1.0f); // Common for points

		REQUIRE(v4.x == 1.0f);
		REQUIRE(v4.y == 2.0f);
		REQUIRE(v4.z == 3.0f);
		REQUIRE(v4.w == 1.0f);
	}

	SECTION("Arithmetic") {
		Vec4f a(1.0f, 2.0f, 3.0f, 4.0f);
		Vec4f b(2.0f, 2.0f, 2.0f, 2.0f);

		a += b;
		REQUIRE(a.x == 3.0f);
		REQUIRE(a.w == 6.0f);
	}
}

TEST_CASE("Type Conversions", "[Vector][Types]") {
	SECTION("Float to Int") {
		Vec2f vf(1.5f, 2.9f);
		Vec2i vi(vf); // Explicit conversion

		REQUIRE(vi.x == 1);
		REQUIRE(vi.y == 2); // Truncation
	}

	SECTION("Double to Float") {
		Vec3d vd(1.0, 2.0, 3.0);
		Vec3f vf(vd);

		REQUIRE(vf.x == 1.0f);
	}
}

TEST_CASE("Edge Cases", "[Vector][Edge]") {
	SECTION("Division by Zero") {
		Vec3f v(10.0f, 10.0f, 10.0f);

		// The implementation sets to max on div by zero
		v /= 0.0f;

		REQUIRE(v.x == std::numeric_limits<float>::max());
		REQUIRE(v.y == std::numeric_limits<float>::max());
		REQUIRE(v.z == std::numeric_limits<float>::max());
	}
}
