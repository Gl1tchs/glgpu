#pragma once

#include "gpukit/matrix.h"
#include "gpukit/vector.h"

#include <cmath>

namespace gpukit {

struct Quat {
	// components: x, y, z (vector part) and w (scalar/real part)
	// Convention: q = w + x*i + y*j + z*k
	float x = 0.0f;
	float y = 0.0f;
	float z = 0.0f;
	float w = 1.0f;

	// Default: Identity quaternion (0, 0, 0, 1)
	Quat() = default;
	Quat(float x, float y, float z, float w) : x(x), y(y), z(z), w(w) {}

	// Create from axis and angle (in radians)
	static Quat angle_axis(float angle_rad, const Vec3f& axis) {
		Vec3f normalized_axis = axis;
		float len_sq = axis.x * axis.x + axis.y * axis.y + axis.z * axis.z;
		if (len_sq > 1e-6f) {
			float len = std::sqrt(len_sq);
			normalized_axis = Vec3f{ axis.x / len, axis.y / len, axis.z / len };
		}
		float half_angle = angle_rad * 0.5f;
		float sin_half = std::sin(half_angle);
		float cos_half = std::cos(half_angle);
		return Quat(normalized_axis.x * sin_half, normalized_axis.y * sin_half,
				normalized_axis.z * sin_half, cos_half);
	}

	// Create from Euler angles (in radians: pitch, yaw, roll)
	static Quat euler(float pitch_rad, float yaw_rad, float roll_rad) {
		float cy = std::cos(yaw_rad * 0.5f);
		float sy = std::sin(yaw_rad * 0.5f);
		float cp = std::cos(pitch_rad * 0.5f);
		float sp = std::sin(pitch_rad * 0.5f);
		float cr = std::cos(roll_rad * 0.5f);
		float sr = std::sin(roll_rad * 0.5f);

		Quat q;
		q.w = cr * cp * cy + sr * sp * sy;
		q.x = sr * cp * cy - cr * sp * sy;
		q.y = cr * sp * cy + sr * cp * sy;
		q.z = cr * cp * sy - sr * sp * cy;
		return q;
	}

	// Length squared
	float length_sq() const { return x * x + y * y + z * z + w * w; }

	// Length
	float length() const { return std::sqrt(length_sq()); }

	// Normalized
	Quat normalized() const {
		float len = length();
		if (len < 1e-6f) {
			return Quat(0.0f, 0.0f, 0.0f, 1.0f);
		}
		return Quat(x / len, y / len, z / len, w / len);
	}

	// Conjugate
	Quat conjugate() const { return Quat(-x, -y, -z, w); }

	// Inverse
	Quat inverse() const {
		float len_sq = length_sq();
		if (len_sq < 1e-6f) {
			return Quat(0.0f, 0.0f, 0.0f, 1.0f);
		}
		float inv_len_sq = 1.0f / len_sq;
		return Quat(-x * inv_len_sq, -y * inv_len_sq, -z * inv_len_sq, w * inv_len_sq);
	}

	// Quaternion multiplication (q1 * q2)
	Quat operator*(const Quat& other) const {
		return Quat(w * other.x + x * other.w + y * other.z - z * other.y,
				w * other.y - x * other.z + y * other.w + z * other.x,
				w * other.z + x * other.y - y * other.x + z * other.w,
				w * other.w - x * other.x - y * other.y - z * other.z);
	}

	// Rotate vector (q * v * q^-1)
	Vec3f operator*(const Vec3f& v) const {
		// Efficient formula: v' = v + 2 * w * (q_xyz x v) + 2 * (q_xyz x (q_xyz x v))
		Vec3f q_xyz{ x, y, z };

		// Cross product of q_xyz and v
		Vec3f cross1{ q_xyz.y * v.z - q_xyz.z * v.y, q_xyz.z * v.x - q_xyz.x * v.z,
			q_xyz.x * v.y - q_xyz.y * v.x };

		// Cross product of q_xyz and cross1
		Vec3f cross2{ q_xyz.y * cross1.z - q_xyz.z * cross1.y,
			q_xyz.z * cross1.x - q_xyz.x * cross1.z, q_xyz.x * cross1.y - q_xyz.y * cross1.x };

		return Vec3f{ v.x + 2.0f * (w * cross1.x + cross2.x),
			v.y + 2.0f * (w * cross1.y + cross2.y), v.z + 2.0f * (w * cross1.z + cross2.z) };
	}

	// Convert to 3x3 rotation matrix (column-major Mat3)
	Mat3 to_mat3() const {
		float xx = x * x;
		float yy = y * y;
		float zz = z * z;
		float xy = x * y;
		float xz = x * z;
		float yz = y * z;
		float wx = w * x;
		float wy = w * y;
		float wz = w * z;

		Mat3 res = Mat3::empty();
		res.cols[0] = Vec3f{ 1.0f - 2.0f * (yy + zz), 2.0f * (xy + wz), 2.0f * (xz - wy) };
		res.cols[1] = Vec3f{ 2.0f * (xy - wz), 1.0f - 2.0f * (xx + zz), 2.0f * (yz + wx) };
		res.cols[2] = Vec3f{ 2.0f * (xz + wy), 2.0f * (yz - wx), 1.0f - 2.0f * (xx + yy) };
		return res;
	}

	// Convert to 4x4 rotation matrix (column-major Mat4)
	Mat4 to_mat4() const {
		float xx = x * x;
		float yy = y * y;
		float zz = z * z;
		float xy = x * y;
		float xz = x * z;
		float yz = y * z;
		float wx = w * x;
		float wy = w * y;
		float wz = w * z;

		Mat4 res = Mat4::empty();
		res.cols[0] = Vec4f{ 1.0f - 2.0f * (yy + zz), 2.0f * (xy + wz), 2.0f * (xz - wy), 0.0f };
		res.cols[1] = Vec4f{ 2.0f * (xy - wz), 1.0f - 2.0f * (xx + zz), 2.0f * (yz + wx), 0.0f };
		res.cols[2] = Vec4f{ 2.0f * (xz + wy), 2.0f * (yz - wx), 1.0f - 2.0f * (xx + yy), 0.0f };
		res.cols[3] = Vec4f{ 0.0f, 0.0f, 0.0f, 1.0f };
		return res;
	}

	// Convert Mat3 rotation matrix to Quaternion
	static Quat from_mat3(const Mat3& m) {
		float tr = m.cols[0][0] + m.cols[1][1] + m.cols[2][2];
		Quat q;
		if (tr > 0.0f) {
			float s = std::sqrt(tr + 1.0f) * 2.0f; // s = 4 * w
			q.w = 0.25f * s;
			q.x = (m.cols[1][2] - m.cols[2][1]) / s;
			q.y = (m.cols[2][0] - m.cols[0][2]) / s;
			q.z = (m.cols[0][1] - m.cols[1][0]) / s;
		} else if ((m.cols[0][0] > m.cols[1][1]) && (m.cols[0][0] > m.cols[2][2])) {
			float s = std::sqrt(1.0f + m.cols[0][0] - m.cols[1][1] - m.cols[2][2]) *
					2.0f; // s = 4 * x
			q.w = (m.cols[1][2] - m.cols[2][1]) / s;
			q.x = 0.25f * s;
			q.y = (m.cols[0][1] + m.cols[1][0]) / s;
			q.z = (m.cols[2][0] + m.cols[0][2]) / s;
		} else if (m.cols[1][1] > m.cols[2][2]) {
			float s = std::sqrt(1.0f + m.cols[1][1] - m.cols[0][0] - m.cols[2][2]) *
					2.0f; // s = 4 * y
			q.w = (m.cols[2][0] - m.cols[0][2]) / s;
			q.x = (m.cols[0][1] + m.cols[1][0]) / s;
			q.y = 0.25f * s;
			q.z = (m.cols[1][2] + m.cols[2][1]) / s;
		} else {
			float s = std::sqrt(1.0f + m.cols[2][2] - m.cols[0][0] - m.cols[1][1]) *
					2.0f; // s = 4 * z
			q.w = (m.cols[0][1] - m.cols[1][0]) / s;
			q.x = (m.cols[2][0] + m.cols[0][2]) / s;
			q.y = (m.cols[1][2] + m.cols[2][1]) / s;
			q.z = 0.25f * s;
		}
		return q.normalized();
	}

	// Spherical Linear Interpolation (SLERP)
	static Quat slerp(const Quat& q1, const Quat& q2, float t) {
		Quat q2_correct = q2;
		float cos_theta = q1.x * q2.x + q1.y * q2.y + q1.z * q2.z + q1.w * q2.w;

		// If the dot product is negative, the quaternions have opposite handed-ness
		// and the shortest path for the interpolation is to change the sign of one of them.
		if (cos_theta < 0.0f) {
			q2_correct = Quat(-q2.x, -q2.y, -q2.z, -q2.w);
			cos_theta = -cos_theta;
		}

		// Clamp cos_theta to prevent domain errors in acos
		if (cos_theta > 0.9995f) {
			// Linear interpolation for very close orientations
			return Quat(q1.x + t * (q2_correct.x - q1.x), q1.y + t * (q2_correct.y - q1.y),
					q1.z + t * (q2_correct.z - q1.z), q1.w + t * (q2_correct.w - q1.w))
					.normalized();
		}

		float theta = std::acos(cos_theta);
		float sin_theta = std::sin(theta);
		float w1 = std::sin((1.0f - t) * theta) / sin_theta;
		float w2 = std::sin(t * theta) / sin_theta;

		return Quat(w1 * q1.x + w2 * q2_correct.x, w1 * q1.y + w2 * q2_correct.y,
				w1 * q1.z + w2 * q2_correct.z, w1 * q1.w + w2 * q2_correct.w);
	}

	bool operator==(const Quat& other) const {
		return std::abs(x - other.x) < 1e-5f && std::abs(y - other.y) < 1e-5f &&
				std::abs(z - other.z) < 1e-5f && std::abs(w - other.w) < 1e-5f;
	}
};

} // namespace gpukit
