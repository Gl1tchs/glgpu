#pragma once

#include "glgpu/math.h"
#include "glgpu/vector.h"

#ifdef GL_USE_SIMD_INTRINSICS
#include <immintrin.h>
#endif

namespace gl {

template <size_t TCols, size_t TRows> struct Mat;

template <> struct Mat<4, 4> {
	std::array<Vec4f, 4> cols;

	// Default: Identity matrix
	Mat(float p_value = 1.0f) :
			cols({
					{ Vec4f{ p_value, 0.0f, 0.0f, 0.0f } },
					{ Vec4f{ 0.0f, p_value, 0.0f, 0.0f } },
					{ Vec4f{ 0.0f, 0.0f, p_value, 0.0f } },
					{ Vec4f{ 0.0f, 0.0f, 0.0f, p_value } },
			}) {}

	// Create empty matrix
	static Mat empty() { return Mat{ {} }; }

	Vec4f& operator[](size_t p_col_idx) { return cols[p_col_idx]; }
	const Vec4f& operator[](size_t p_col_idx) const { return cols[p_col_idx]; }

	Mat operator+(const Mat& p_other) const {
		Mat res;
#ifdef GL_USE_SIMD_INTRINSICS
		// Load, Add, Store for each column
		for (int i = 0; i < 4; ++i) {
			// Unaligned loads are safe and fast on modern CPUs
			__m128 a = _mm_loadu_ps(&cols[i].x);
			__m128 b = _mm_loadu_ps(&p_other.cols[i].x);
			__m128 r = _mm_add_ps(a, b);
			_mm_storeu_ps(&res.cols[i].x, r);
		}
#else
		for (size_t c = 0; c < 4; ++c) {
			for (size_t r = 0; r < 4; ++r) {
				res.cols[c][r] = cols[c][r] + p_other.cols[c][r];
			}
		}
#endif
		return res;
	}

	Mat operator-(const Mat& p_other) const {
		Mat res;
#ifdef GL_USE_SIMD_INTRINSICS
		for (int i = 0; i < 4; ++i) {
			__m128 a = _mm_loadu_ps(&cols[i].x);
			__m128 b = _mm_loadu_ps(&p_other.cols[i].x);
			__m128 r = _mm_sub_ps(a, b);
			_mm_storeu_ps(&res.cols[i].x, r);
		}
#else
		for (size_t c = 0; c < 4; ++c) {
			for (size_t r = 0; r < 4; ++r) {
				res.cols[c][r] = cols[c][r] - p_other.cols[c][r];
			}
		}
#endif
		return res;
	}

	Mat operator*(const Mat& p_other) const {
		Mat res;
#ifdef GL_USE_SIMD_INTRINSICS
		// Load columns of 'this' matrix into registers
		__m128 Col0 = _mm_loadu_ps(&cols[0].x);
		__m128 Col1 = _mm_loadu_ps(&cols[1].x);
		__m128 Col2 = _mm_loadu_ps(&cols[2].x);
		__m128 Col3 = _mm_loadu_ps(&cols[3].x);

		for (int i = 0; i < 4; ++i) {
			// Load one column from the 'other' matrix
			__m128 OtherCol = _mm_loadu_ps(&p_other.cols[i].x);

			// Broadcast the components of OtherCol:
			// xxxx, yyyy, zzzz, wwww
			__m128 e0 = _mm_shuffle_ps(OtherCol, OtherCol, _MM_SHUFFLE(0, 0, 0, 0));
			__m128 e1 = _mm_shuffle_ps(OtherCol, OtherCol, _MM_SHUFFLE(1, 1, 1, 1));
			__m128 e2 = _mm_shuffle_ps(OtherCol, OtherCol, _MM_SHUFFLE(2, 2, 2, 2));
			__m128 e3 = _mm_shuffle_ps(OtherCol, OtherCol, _MM_SHUFFLE(3, 3, 3, 3));

			// Linear Combination:
			// Res = (Col0 * x) + (Col1 * y) + (Col2 * z) + (Col3 * w)
			__m128 m0 = _mm_mul_ps(Col0, e0);
			__m128 m1 = _mm_mul_ps(Col1, e1);
			__m128 m2 = _mm_mul_ps(Col2, e2);
			__m128 m3 = _mm_mul_ps(Col3, e3);

			__m128 sum = _mm_add_ps(_mm_add_ps(m0, m1), _mm_add_ps(m2, m3));

			_mm_storeu_ps(&res.cols[i].x, sum);
		}
#else
		// Fallback: Clear to 0 first or use constructor
		res = Mat::empty();
		for (size_t c = 0; c < 4; ++c) {
			for (size_t r = 0; r < 4; ++r) {
				res.cols[c][r] = cols[0][r] * p_other.cols[c][0] + cols[1][r] * p_other.cols[c][1] +
						cols[2][r] * p_other.cols[c][2] + cols[3][r] * p_other.cols[c][3];
			}
		}
#endif
		return res;
	}

	// Matrix * Vector multiplication
	Vec4f operator*(const Vec4f& p_v) const {
#ifdef GL_USE_SIMD_INTRINSICS
		// Load columns of matrix
		__m128 Col0 = _mm_loadu_ps(&cols[0].x);
		__m128 Col1 = _mm_loadu_ps(&cols[1].x);
		__m128 Col2 = _mm_loadu_ps(&cols[2].x);
		__m128 Col3 = _mm_loadu_ps(&cols[3].x);

		// Load the vector
		__m128 vec = _mm_loadu_ps(&p_v.x);

		// Broadcast vector components
		__m128 v0 = _mm_shuffle_ps(vec, vec, _MM_SHUFFLE(0, 0, 0, 0));
		__m128 v1 = _mm_shuffle_ps(vec, vec, _MM_SHUFFLE(1, 1, 1, 1));
		__m128 v2 = _mm_shuffle_ps(vec, vec, _MM_SHUFFLE(2, 2, 2, 2));
		__m128 v3 = _mm_shuffle_ps(vec, vec, _MM_SHUFFLE(3, 3, 3, 3));

		// Linear combination
		__m128 m0 = _mm_mul_ps(Col0, v0);
		__m128 m1 = _mm_mul_ps(Col1, v1);
		__m128 m2 = _mm_mul_ps(Col2, v2);
		__m128 m3 = _mm_mul_ps(Col3, v3);

		__m128 res_vec = _mm_add_ps(_mm_add_ps(m0, m1), _mm_add_ps(m2, m3));

		Vec4f res;
		_mm_storeu_ps(&res.x, res_vec);
		return res;
#else
		Vec4f res;
		res.x = cols[0][0] * p_v.x + cols[1][0] * p_v.y + cols[2][0] * p_v.z + cols[3][0] * p_v.w;
		res.y = cols[0][1] * p_v.x + cols[1][1] * p_v.y + cols[2][1] * p_v.z + cols[3][1] * p_v.w;
		res.z = cols[0][2] * p_v.x + cols[1][2] * p_v.y + cols[2][2] * p_v.z + cols[3][2] * p_v.w;
		res.w = cols[0][3] * p_v.x + cols[1][3] * p_v.y + cols[2][3] * p_v.z + cols[3][3] * p_v.w;
		return res;
#endif
	}

	bool operator==(const Mat& p_other) const {
		for (size_t c = 0; c < 4; ++c) {
			for (size_t r = 0; r < 4; ++r) {
				if (std::abs(cols[c][r] - p_other.cols[c][r]) > 1e-6f) {
					return false;
				}
			}
		}
		return true;
	}

	// Linalg utilities

	Mat transpose() const {
		Mat res = Mat::empty();
		for (size_t c = 0; c < 4; ++c) {
			for (size_t r = 0; r < 4; ++r) {
				res.cols[r][c] = cols[c][r];
			}
		}
		return res;
	}

	// Calculates 3x3 sub-determinant
	float minor(
			size_t p_c0, size_t p_c1, size_t p_c2, size_t p_r0, size_t p_r1, size_t p_r2) const {
		return cols[p_c0][p_r0] *
				(cols[p_c1][p_r1] * cols[p_c2][p_r2] - cols[p_c2][p_r1] * cols[p_c1][p_r2]) -
				cols[p_c1][p_r0] *
				(cols[p_c0][p_r1] * cols[p_c2][p_r2] - cols[p_c2][p_r1] * cols[p_c0][p_r2]) +
				cols[p_c2][p_r0] *
				(cols[p_c0][p_r1] * cols[p_c1][p_r2] - cols[p_c1][p_r1] * cols[p_c0][p_r2]);
	}

	float determinant() const {
		return cols[0][0] * minor(1, 2, 3, 1, 2, 3) - cols[1][0] * minor(0, 2, 3, 1, 2, 3) +
				cols[2][0] * minor(0, 1, 3, 1, 2, 3) - cols[3][0] * minor(0, 1, 2, 1, 2, 3);
	}

	Mat inverse() const {
		const float det = determinant();
		if (std::abs(det) < 1e-6f) {
			return Mat::empty();
		}

		Mat res = Mat::empty();
		float inv_det = 1.0f / det;

		// Co-factors / Adjugate matrix
		// Row 0
		res.cols[0][0] = minor(1, 2, 3, 1, 2, 3) * inv_det;
		res.cols[0][1] = -minor(0, 2, 3, 1, 2, 3) * inv_det; // Transposed assignment for adjugate
		res.cols[0][2] = minor(0, 1, 3, 1, 2, 3) * inv_det;
		res.cols[0][3] = -minor(0, 1, 2, 1, 2, 3) * inv_det;

		// Row 1 (Note: cols indices swapped for transpose effect)
		res.cols[1][0] = -minor(1, 2, 3, 0, 2, 3) * inv_det;
		res.cols[1][1] = minor(0, 2, 3, 0, 2, 3) * inv_det;
		res.cols[1][2] = -minor(0, 1, 3, 0, 2, 3) * inv_det;
		res.cols[1][3] = minor(0, 1, 2, 0, 2, 3) * inv_det;

		// Row 2
		res.cols[2][0] = minor(1, 2, 3, 0, 1, 3) * inv_det;
		res.cols[2][1] = -minor(0, 2, 3, 0, 1, 3) * inv_det;
		res.cols[2][2] = minor(0, 1, 3, 0, 1, 3) * inv_det;
		res.cols[2][3] = -minor(0, 1, 2, 0, 1, 3) * inv_det;

		// Row 3
		res.cols[3][0] = -minor(1, 2, 3, 0, 1, 2) * inv_det;
		res.cols[3][1] = minor(0, 2, 3, 0, 1, 2) * inv_det;
		res.cols[3][2] = -minor(0, 1, 3, 0, 1, 2) * inv_det;
		res.cols[3][3] = minor(0, 1, 2, 0, 1, 2) * inv_det;

		return res;
	}

	// Transformations

	// Creates a translation matrix
	static Mat translate(Vec3f p_translation) {
		Mat res; // Identity
		res.cols[3][0] = p_translation.x;
		res.cols[3][1] = p_translation.y;
		res.cols[3][2] = p_translation.z;
		return res;
	}

	// Creates a rotation matrix (angle in radians, axis normalized)
	static Mat rotate(float p_angle_rad, Vec3f p_axis) {
		Mat res;
		float c = std::cos(p_angle_rad);
		float s = std::sin(p_angle_rad);
		float omc = 1.0f - c;

		res.cols[0][0] = p_axis.x * p_axis.x * omc + c;
		res.cols[0][1] = p_axis.y * p_axis.x * omc + p_axis.z * s;
		res.cols[0][2] = p_axis.x * p_axis.z * omc - p_axis.y * s;

		res.cols[1][0] = p_axis.x * p_axis.y * omc - p_axis.z * s;
		res.cols[1][1] = p_axis.y * p_axis.y * omc + c;
		res.cols[1][2] = p_axis.y * p_axis.z * omc + p_axis.x * s;

		res.cols[2][0] = p_axis.x * p_axis.z * omc + p_axis.y * s;
		res.cols[2][1] = p_axis.y * p_axis.z * omc - p_axis.x * s;
		res.cols[2][2] = p_axis.z * p_axis.z * omc + c;

		return res;
	}

	// Turn euler angles to rotation matrix
	static Mat from_euler_angles(const Vec3f& p_euler_degrees) {
		const float pitch = as_radians(p_euler_degrees.x); // Pitch
		const float yaw = as_radians(p_euler_degrees.y); // Yaw
		const float roll = as_radians(p_euler_degrees.z); // Roll

		// Rotation order: Z * X * Y
		// Here we use the GLM-like composition: Mat = Mat_Z * Mat_X * Mat_Y
		Mat mat_x = Mat::rotate(pitch, { 1.0f, 0.0f, 0.0f });
		Mat mat_y = Mat::rotate(yaw, { 0.0f, 1.0f, 0.0f });
		Mat mat_z = Mat::rotate(roll, { 0.0f, 0.0f, 1.0f });

		// This composition order will match the original GLM implementation's
		// quaternion behavior if the quaternion was constructed from the same order.
		return mat_y * mat_x * mat_z;
	}

	// Creates a scale matrix
	static Mat scale(Vec3f p_scale) {
		Mat res;
		res[0][0] = p_scale.x;
		res[1][1] = p_scale.y;
		res[2][2] = p_scale.z;
		return res;
	}

	// Projections

	// LookAt (Right-Handed)
	static Mat look_at(Vec3f p_eye, Vec3f p_center, Vec3f p_up) {
		Vec3f f = (p_center - p_eye).normalize(); // Forward
		Vec3f s = f.cross(p_up).normalize(); // Right
		Vec3f u = s.cross(f); // True Up

		Mat res;
		// Rotation part
		res.cols[0][0] = s.x;
		res.cols[1][0] = s.y;
		res.cols[2][0] = s.z;
		res.cols[0][1] = u.x;
		res.cols[1][1] = u.y;
		res.cols[2][1] = u.z;
		res.cols[0][2] = -f.x;
		res.cols[1][2] = -f.y;
		res.cols[2][2] = -f.z;

		// Translation part (dot products)
		res.cols[3][0] = -s.dot(p_eye);
		res.cols[3][1] = -u.dot(p_eye);
		res.cols[3][2] = f.dot(p_eye);

		return res;
	}

	// Orthographic Projection
	static Mat ortho(float p_left, float p_right, float p_bottom, float p_top, float p_z_near,
			float p_z_far) {
		Mat res; // Identity
		res.cols[0][0] = 2.0f / (p_right - p_left);
		res.cols[1][1] = 2.0f / (p_top - p_bottom);
		res.cols[2][2] = -2.0f / (p_z_far - p_z_near);

		res.cols[3][0] = -(p_right + p_left) / (p_right - p_left);
		res.cols[3][1] = -(p_top + p_bottom) / (p_top - p_bottom);
		res.cols[3][2] = -(p_z_far + p_z_near) / (p_z_far - p_z_near);
		return res;
	}

	// Perspective Projection (FOV in radians)
	static Mat perspective(float p_fovy_rad, float p_aspect, float p_z_near, float p_z_far) {
		Mat res(0.0f); // Zero init, not identity

		float const tan_half_fovy = std::tan(p_fovy_rad / 2.0f);

		res.cols[0][0] = 1.0f / (p_aspect * tan_half_fovy);
		res.cols[1][1] = 1.0f / (tan_half_fovy);
		res.cols[2][2] = -(p_z_far + p_z_near) / (p_z_far - p_z_near);
		res.cols[2][3] = -1.0f;
		res.cols[3][2] = -(2.0f * p_z_far * p_z_near) / (p_z_far - p_z_near);

		return res;
	}
};

typedef Mat<4, 4> Mat4;

}; //namespace gl
