#pragma once

#include "glgpu/math.h"
#include "glgpu/vec.h"

namespace gl {

template <size_t TCols, size_t TRows> struct Mat;

template <> struct Mat<4, 4> {
	std::array<std::array<float, 4>, 4> cols;

	// Default: Identity matrix
	Mat(float p_value = 1.0f) :
			cols({
					{ { p_value, 0.0f, 0.0f, 0.0f } },
					{ { 0.0f, p_value, 0.0f, 0.0f } },
					{ { 0.0f, 0.0f, p_value, 0.0f } },
					{ { 0.0f, 0.0f, 0.0f, p_value } },
			}) {}

	// Create empty matrix
	static Mat empty() { return Mat{ {} }; }

	std::array<float, 4>& operator[](size_t p_col_idx) { return cols[p_col_idx]; }
	const std::array<float, 4>& operator[](size_t p_col_idx) const { return cols[p_col_idx]; }

	Mat operator+(const Mat& p_other) const {
		Mat res;
		for (int c = 0; c < 4; ++c) {
			for (int r = 0; r < 4; ++r) {
				res.cols[c][r] = cols[c][r] + p_other.cols[c][r];
			}
		}
		return res;
	}

	Mat operator-(const Mat& p_other) const {
		Mat res;
		for (int c = 0; c < 4; ++c) {
			for (int r = 0; r < 4; ++r) {
				res.cols[c][r] = cols[c][r] - p_other.cols[c][r];
			}
		}
		return res;
	}

	Mat operator*(const Mat& p_other) const {
		Mat res = Mat::empty();
		for (int c = 0; c < 4; ++c) {
			for (int r = 0; r < 4; ++r) {
				// Dot product of (this Row r) and (other Col c)
				res.cols[c][r] = cols[0][r] * p_other.cols[c][0] + cols[1][r] * p_other.cols[c][1] +
						cols[2][r] * p_other.cols[c][2] + cols[3][r] * p_other.cols[c][3];
			}
		}
		return res;
	}

	// Matrix * Vector Multiplication
	Vec4f operator*(const Vec4f& p_v) const {
		Vec4f res;
		// Linear combination of columns
		res.x = cols[0][0] * p_v.x + cols[1][0] * p_v.y + cols[2][0] * p_v.z + cols[3][0] * p_v.w;
		res.y = cols[0][1] * p_v.x + cols[1][1] * p_v.y + cols[2][1] * p_v.z + cols[3][1] * p_v.w;
		res.z = cols[0][2] * p_v.x + cols[1][2] * p_v.y + cols[2][2] * p_v.z + cols[3][2] * p_v.w;
		res.w = cols[0][3] * p_v.x + cols[1][3] * p_v.y + cols[2][3] * p_v.z + cols[3][3] * p_v.w;
		return res;
	}

	bool operator==(const Mat& p_other) const {
		for (int c = 0; c < 4; ++c) {
			for (int r = 0; r < 4; ++r) {
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
		for (int c = 0; c < 4; ++c) {
			for (int r = 0; r < 4; ++r) {
				res.cols[r][c] = cols[c][r];
			}
		}
		return res;
	}

	// Calculates 3x3 sub-determinant
	float minor(int p_c0, int p_c1, int p_c2, int p_r0, int p_r1, int p_r2) const {
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
		if (std::abs(det) < 1e-6f)
			return Mat::empty();

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
	static Mat ortho(float left, float right, float bottom, float top, float zNear, float zFar) {
		Mat res; // Identity
		res.cols[0][0] = 2.0f / (right - left);
		res.cols[1][1] = 2.0f / (top - bottom);
		res.cols[2][2] = -2.0f / (zFar - zNear);

		res.cols[3][0] = -(right + left) / (right - left);
		res.cols[3][1] = -(top + bottom) / (top - bottom);
		res.cols[3][2] = -(zFar + zNear) / (zFar - zNear);
		return res;
	}

	// Perspective Projection (FOV in radians)
	static Mat perspective(float fovyRad, float aspect, float zNear, float zFar) {
		Mat res(0.0f); // Zero init, not identity

		float const tanHalfFovy = std::tan(fovyRad / 2.0f);

		res.cols[0][0] = 1.0f / (aspect * tanHalfFovy);
		res.cols[1][1] = 1.0f / (tanHalfFovy);
		res.cols[2][2] = -(zFar + zNear) / (zFar - zNear);
		res.cols[2][3] = -1.0f;
		res.cols[3][2] = -(2.0f * zFar * zNear) / (zFar - zNear);

		return res;
	}
};

typedef Mat<4, 4> Mat4;

}; //namespace gl
