#include <catch2/catch_test_macros.hpp>

// #define GL_USE_SIMD_INTRINSICS

#include "gpukit/matrix.h"

using namespace gpukit;

const float PI = 3.14159265359f;

TEST_CASE("Matrix Construction and Access", "[Mat4]") {
	SECTION("Default Constructor creates Identity Matrix") {
		Mat4 m;
		// Check Diagonal
		REQUIRE(m[0][0] == 1.0f);
		REQUIRE(m[1][1] == 1.0f);
		REQUIRE(m[2][2] == 1.0f);
		REQUIRE(m[3][3] == 1.0f);

		// Check off-diagonal (sample)
		REQUIRE(m[0][1] == 0.0f);
		REQUIRE(m[3][0] == 0.0f);
	}

	SECTION("Empty creates Zero Matrix") {
		Mat4 m = Mat4::empty();
		for (int c = 0; c < 4; c++) {
			for (int r = 0; r < 4; r++) {
				REQUIRE(m[c][r] == 0.0f);
			}
		}
	}
}

TEST_CASE("Matrix Arithmetic Operations", "[Mat4][SIMD]") {
	Mat4 a(2.0f); // Diagonal 2.0
	Mat4 b(3.0f); // Diagonal 3.0

	SECTION("Addition") {
		Mat4 res = a + b;
		REQUIRE(res[0][0] == 5.0f);
		REQUIRE(res[1][1] == 5.0f);
		REQUIRE(res[3][3] == 5.0f);
		REQUIRE(res[1][0] == 0.0f);
	}

	SECTION("Subtraction") {
		Mat4 res = b - a;
		REQUIRE(res[0][0] == 1.0f);
		REQUIRE(res[3][3] == 1.0f);
	}

	SECTION("Matrix Multiplication (Diagonal)") {
		// [2 0] * [3 0] = [6 0]
		// [0 2]   [0 3]   [0 6]
		Mat4 res = a * b;
		REQUIRE(res[0][0] == 6.0f);
		REQUIRE(res[3][3] == 6.0f);
		REQUIRE(res[0][1] == 0.0f);
	}

	SECTION("Matrix Multiplication (Complex)") {
		// Row-major visual:
		// A = [1 2 3 4]    B = Identity
		//     [5 6 7 8]
		//     ...
		// Constructing manually column-by-column
		Mat4 m1 = Mat4::empty();
		m1.cols[0] = { 1, 5, 9, 13 };
		m1.cols[1] = { 2, 6, 10, 14 };
		m1.cols[2] = { 3, 7, 11, 15 };
		m1.cols[3] = { 4, 8, 12, 16 };

		Mat4 m2 = Mat4(1.0f); // Identity

		Mat4 res = m1 * m2;
		REQUIRE(res == m1);

		Mat4 zero = Mat4::empty();
		REQUIRE((m1 * zero) == zero);
	}
}

TEST_CASE("Matrix-Vector Multiplication", "[Mat4][Vec4]") {
	SECTION("Identity Multiplication") {
		Mat4 id(1.0f);
		Vec4f v(1.0f, 2.0f, 3.0f, 1.0f);
		Vec4f res = id * v;

		REQUIRE(res.x == 1.0f);
		REQUIRE(res.y == 2.0f);
		REQUIRE(res.z == 3.0f);
		REQUIRE(res.w == 1.0f);
	}

	SECTION("Translation Multiplication") {
		// Translate by (10, 20, 30)
		Mat4 trans = Mat4::translate({ 10.0f, 20.0f, 30.0f });
		Vec4f point(5.0f, 5.0f, 5.0f, 1.0f); // w=1 for point

		Vec4f res = trans * point;
		REQUIRE(res.x == 15.0f);
		REQUIRE(res.y == 25.0f);
		REQUIRE(res.z == 35.0f);
		REQUIRE(res.w == 1.0f);
	}

	SECTION("Scale Multiplication") {
		Mat4 scale = Mat4::scale({ 2.0f, 0.5f, 0.0f });
		Vec4f v(10.0f, 10.0f, 10.0f, 1.0f);

		Vec4f res = scale * v;
		REQUIRE(res.x == 20.0f);
		REQUIRE(res.y == 5.0f);
		REQUIRE(res.z == 0.0f);
	}
}

TEST_CASE("Linear Algebra Utilities", "[Mat4][Math]") {
	SECTION("Transpose") {
		Mat4 m = Mat4::empty();
		m.cols[0] = { 1, 2, 3, 4 }; // Column 0

		Mat4 t = m.transpose();
		// Old Col0 becomes New Row0
		REQUIRE(t.cols[0][0] == 1.0f);
		REQUIRE(t.cols[1][0] == 2.0f);
		REQUIRE(t.cols[2][0] == 3.0f);
		REQUIRE(t.cols[3][0] == 4.0f);
	}

	SECTION("Determinant") {
		Mat4 id(1.0f);
		REQUIRE(id.determinant() == 1.0f);

		Mat4 scale = Mat4::scale({ 2.0f, 2.0f, 2.0f });
		// Det = 2 * 2 * 2 * 1 (w)
		REQUIRE(scale.determinant() == 8.0f);
	}

	SECTION("Inverse") {
		Mat4 id(1.0f);
		REQUIRE(id.inverse() == id);

		// Inverse of scaling (2,2,2) should be scaling (0.5, 0.5, 0.5)
		Mat4 scale = Mat4::scale({ 2.0f, 2.0f, 2.0f });
		Mat4 inv = scale.inverse();

		REQUIRE(inv[0][0] == 0.5f);
		REQUIRE(inv[1][1] == 0.5f);
		REQUIRE(inv[2][2] == 0.5f);

		// Verification property: M * M_inv = I
		Mat4 trans = Mat4::translate({ 5.0f, -3.0f, 2.0f });
		Mat4 res = trans * trans.inverse();
		REQUIRE(res == Mat4(1.0f));
	}
}

TEST_CASE("Transformations", "[Mat4][Transform]") {
	SECTION("Rotation (Z-Axis)") {
		// Rotate 90 degrees around Z
		// X (1,0,0) becomes Y (0,1,0)
		Mat4 rot = Mat4::rotate(PI / 2.0f, { 0.0f, 0.0f, 1.0f });
		Vec4f v(1.0f, 0.0f, 0.0f, 1.0f);

		Vec4f res = rot * v;
		// Allow small epsilon for trig approximation
		REQUIRE(std::abs(res.x) < 1e-5f); // ~0
		REQUIRE(std::abs(res.y - 1.0f) < 1e-5f); // ~1
	}

	SECTION("LookAt") {
		Vec3f eye(0.0f, 0.0f, 10.0f);
		Vec3f center(0.0f, 0.0f, 0.0f);
		Vec3f up(0.0f, 1.0f, 0.0f);

		Mat4 view = Mat4::look_at(eye, center, up);

		// View matrix transforms world to camera space.
		// The eye position (0,0,10) should map to (0,0,0) or relevant depth depending on
		// convention, but essentially it moves the world back by 10 units on Z.

		Vec4f world_origin(0.0f, 0.0f, 0.0f, 1.0f);
		Vec4f view_pos = view * world_origin;

		REQUIRE(view_pos.z == -10.0f); // Moved into camera view
	}
}

TEST_CASE("Projections", "[Mat4][Projection]") {
	SECTION("Perspective") {
		float fov = PI / 2.0f; // 90 deg
		float aspect = 1.0f; // Square
		float znear = 0.1f;
		float zfar = 100.0f;

		Mat4 proj = Mat4::perspective(fov, aspect, znear, zfar);

		// Test point on near plane centered
		Vec4f p_near(0.0f, 0.0f, -0.1f, 1.0f);
		// Note: OpenGL convention is looking down -Z.

		Vec4f res = proj * p_near;

		// After perspective divide (x/w, y/w, z/w), it should be NDC.
		// Here we just check w component, which for perspective is usually -z
		REQUIRE(res.w == 0.1f);
	}

	SECTION("Orthographic") {
		Mat4 ortho = Mat4::ortho(-10.0f, 10.0f, -10.0f, 10.0f, -1.0f, 1.0f);
		Vec4f v(10.0f, 0.0f, 0.0f, 1.0f); // Right edge

		Vec4f res = ortho * v;
		// Should map to x = 1.0 in NDC
		REQUIRE(res.x == 1.0f);
	}
}

TEST_CASE("Matrix3 Construction and Arithmetic", "[Mat3]") {
	SECTION("Default Constructor creates Identity Matrix") {
		Mat3 m;
		REQUIRE(m[0][0] == 1.0f);
		REQUIRE(m[1][1] == 1.0f);
		REQUIRE(m[2][2] == 1.0f);
		REQUIRE(m[0][1] == 0.0f);
		REQUIRE(m[2][0] == 0.0f);
	}

	SECTION("Empty creates Zero Matrix") {
		Mat3 m = Mat3::empty();
		for (int c = 0; c < 3; c++) {
			for (int r = 0; r < 3; r++) {
				REQUIRE(m[c][r] == 0.0f);
			}
		}
	}

	SECTION("Addition and Subtraction") {
		Mat3 a(2.0f);
		Mat3 b(3.0f);
		Mat3 sum = a + b;
		Mat3 diff = b - a;
		REQUIRE(sum[0][0] == 5.0f);
		REQUIRE(sum[2][2] == 5.0f);
		REQUIRE(diff[0][0] == 1.0f);
		REQUIRE(diff[2][2] == 1.0f);
	}

	SECTION("Multiplication and Inverse") {
		Mat3 m = Mat3::empty();
		m.cols[0] = { 1.0f, 0.0f, 2.0f };
		m.cols[1] = { 0.0f, 3.0f, 0.0f };
		m.cols[2] = { 4.0f, 0.0f, 5.0f };

		// Determinant: 1 * (3 * 5 - 0 * 0) - 0 + 4 * (0 * 0 - 3 * 2) = 15 - 24 = -9
		REQUIRE(std::abs(m.determinant() - (-9.0f)) < 1e-5f);

		Mat3 inv = m.inverse();
		Mat3 id = m * inv;
		REQUIRE(id == Mat3(1.0f));

		Vec3f v(1.0f, 2.0f, 3.0f);
		Vec3f v_res = m * v;
		// m * v:
		// row 0: 1 * 1 + 0 * 2 + 4 * 3 = 13
		// row 1: 0 * 1 + 3 * 2 + 0 * 3 = 6
		// row 2: 2 * 1 + 0 * 2 + 5 * 3 = 17
		REQUIRE(v_res.x == 13.0f);
		REQUIRE(v_res.y == 6.0f);
		REQUIRE(v_res.z == 17.0f);
	}
}

TEST_CASE("Matrix2 Construction and Arithmetic", "[Mat2]") {
	SECTION("Default Constructor creates Identity Matrix") {
		Mat2 m;
		REQUIRE(m[0][0] == 1.0f);
		REQUIRE(m[1][1] == 1.0f);
		REQUIRE(m[0][1] == 0.0f);
		REQUIRE(m[1][0] == 0.0f);
	}

	SECTION("Empty creates Zero Matrix") {
		Mat2 m = Mat2::empty();
		for (int c = 0; c < 2; c++) {
			for (int r = 0; r < 2; r++) {
				REQUIRE(m[c][r] == 0.0f);
			}
		}
	}

	SECTION("Addition and Subtraction") {
		Mat2 a(2.0f);
		Mat2 b(3.0f);
		Mat2 sum = a + b;
		Mat2 diff = b - a;
		REQUIRE(sum[0][0] == 5.0f);
		REQUIRE(sum[1][1] == 5.0f);
		REQUIRE(diff[0][0] == 1.0f);
		REQUIRE(diff[1][1] == 1.0f);
	}

	SECTION("Multiplication") {
		Mat2 m = Mat2::empty();
		m.cols[0] = { 1.0f, 2.0f };
		m.cols[1] = { 3.0f, 4.0f };

		Vec2f v(1.0f, 2.0f);
		Vec2f v_res = m * v;
		// m * v:
		// row 0: 1 * 1 + 3 * 2 = 7
		// row 1: 2 * 1 + 4 * 2 = 10
		REQUIRE(v_res.x == 7.0f);
		REQUIRE(v_res.y == 10.0f);

		Mat2 m2 = Mat2::empty();
		m2.cols[0] = { 2.0f, 0.0f };
		m2.cols[1] = { 0.0f, 3.0f };
		Mat2 m_res = m * m2;
		// m * m2:
		// col 0: col 0 of m * 2 = { 2, 4 }
		// col 1: col 1 of m * 3 = { 9, 12 }
		REQUIRE(m_res.cols[0].x == 2.0f);
		REQUIRE(m_res.cols[0].y == 4.0f);
		REQUIRE(m_res.cols[1].x == 9.0f);
		REQUIRE(m_res.cols[1].y == 12.0f);
	}
}
