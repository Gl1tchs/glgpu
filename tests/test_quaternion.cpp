#include <catch2/catch_test_macros.hpp>

#include "glgpu/glgpu.h"

#include <cmath>

TEST_CASE("Quaternion Construction and Basic Properties", "[Quat]") {
	SECTION("Default Constructor creates Identity Quaternion") {
		gl::Quat q;
		REQUIRE(q.x == 0.0f);
		REQUIRE(q.y == 0.0f);
		REQUIRE(q.z == 0.0f);
		REQUIRE(q.w == 1.0f);
		REQUIRE(q.length() == 1.0f);
	}

	SECTION("Length and Normalization") {
		gl::Quat q(1.0f, 2.0f, 3.0f, 4.0f);
		float expected_len = std::sqrt(1.0f + 4.0f + 9.0f + 16.0f);
		REQUIRE(std::abs(q.length() - expected_len) < 1e-5f);

		gl::Quat q_norm = q.normalized();
		REQUIRE(std::abs(q_norm.length() - 1.0f) < 1e-5f);
	}

	SECTION("Conjugate and Inverse") {
		gl::Quat q(1.0f, 2.0f, 3.0f, 4.0f);
		gl::Quat conj = q.conjugate();
		REQUIRE(conj.x == -1.0f);
		REQUIRE(conj.y == -2.0f);
		REQUIRE(conj.z == -3.0f);
		REQUIRE(conj.w == 4.0f);

		gl::Quat inv = q.inverse();
		gl::Quat prod = q * inv;
		REQUIRE(std::abs(prod.x) < 1e-5f);
		REQUIRE(std::abs(prod.y) < 1e-5f);
		REQUIRE(std::abs(prod.z) < 1e-5f);
		REQUIRE(std::abs(prod.w - 1.0f) < 1e-5f);
	}
}

TEST_CASE("Quaternion Rotations", "[Quat]") {
	SECTION("Axis-Angle rotation of 90 degrees around Z axis") {
		// Rotate Vec3f(1, 0, 0) by 90 degrees (pi/2 radians) around Z axis -> should be (0, 1, 0)
		gl::Quat q = gl::Quat::angle_axis(M_PI / 2.0f, gl::Vec3f{ 0.0f, 0.0f, 1.0f });
		gl::Vec3f v(1.0f, 0.0f, 0.0f);
		gl::Vec3f rotated = q * v;

		REQUIRE(std::abs(rotated.x) < 1e-5f);
		REQUIRE(std::abs(rotated.y - 1.0f) < 1e-5f);
		REQUIRE(std::abs(rotated.z) < 1e-5f);
	}

	SECTION("Euler Angles pitch/yaw/roll") {
		// Rotate 90 degrees around Z (yaw = 90 deg)
		gl::Quat q = gl::Quat::euler(0.0f, M_PI / 2.0f, 0.0f);
		gl::Vec3f v(1.0f, 0.0f, 0.0f);
		gl::Vec3f rotated = q * v;

		REQUIRE(std::abs(rotated.x) < 1e-5f);
		REQUIRE(std::abs(rotated.y - 1.0f) < 1e-5f);
		REQUIRE(std::abs(rotated.z) < 1e-5f);
	}

	SECTION("Conversion to Matrix and Back") {
		gl::Quat q1 = gl::Quat::angle_axis(0.5f, gl::Vec3f{ 1.0f, 2.0f, 3.0f }).normalized();
		gl::Mat3 m = q1.to_mat3();
		gl::Quat q2 = gl::Quat::from_mat3(m);

		// Quaternions represent double cover of SO(3), so q and -q represent same rotation.
		// We check for equality of q1 and q2 or q1 and -q2
		bool match = (q1 == q2) || (q1 == gl::Quat(-q2.x, -q2.y, -q2.z, -q2.w));
		REQUIRE(match);
	}

	SECTION("Slerp Interpolation") {
		gl::Quat q0 = gl::Quat::angle_axis(0.0f, gl::Vec3f{ 0.0f, 0.0f, 1.0f });
		gl::Quat q1 = gl::Quat::angle_axis(M_PI / 2.0f, gl::Vec3f{ 0.0f, 0.0f, 1.0f });
		gl::Quat q_half = gl::Quat::slerp(q0, q1, 0.5f);

		gl::Quat expected = gl::Quat::angle_axis(M_PI / 4.0f, gl::Vec3f{ 0.0f, 0.0f, 1.0f });
		REQUIRE(q_half == expected);
	}
}
