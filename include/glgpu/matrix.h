#pragma once

#include "glgpu/math.h"
#include "glgpu/vector.h"

#include <array>

#ifdef GL_USE_SIMD_INTRINSICS
#include <immintrin.h>
#endif

namespace gl {

template <size_t TCols, size_t TRows> struct Mat;

#ifdef GL_USE_SIMD_INTRINSICS
inline __m128 load_vec3(const float* ptr) {
	__m128 xy = _mm_castpd_ps(_mm_load_sd(reinterpret_cast<const double*>(ptr)));
	__m128 z = _mm_load_ss(ptr + 2);
	return _mm_movelh_ps(xy, z);
}

inline void store_vec3(float* ptr, __m128 v) {
	_mm_storel_pi(reinterpret_cast<__m64*>(ptr), v);
	_mm_store_ss(ptr + 2, _mm_shuffle_ps(v, v, _MM_SHUFFLE(2, 2, 2, 2)));
}
#endif

template <size_t N> struct Mat<N, N> {
	std::array<Vec<N, float>, N> cols;

	// Default: Identity matrix
	Mat(float value = 1.0f) {
		for (size_t c = 0; c < N; ++c) {
			for (size_t r = 0; r < N; ++r) {
				cols[c][r] = (c == r) ? value : 0.0f;
			}
		}
	}

	// Create empty matrix
	static Mat empty() {
		Mat m;
		for (size_t c = 0; c < N; ++c) {
			for (size_t r = 0; r < N; ++r) {
				m.cols[c][r] = 0.0f;
			}
		}
		return m;
	}

	Vec<N, float>& operator[](size_t col_idx) { return cols[col_idx]; }
	const Vec<N, float>& operator[](size_t col_idx) const { return cols[col_idx]; }

	Mat operator+(const Mat& other) const {
#ifdef GL_USE_SIMD_INTRINSICS
		if constexpr (N == 2) {
			__m128 a = _mm_loadu_ps(&cols[0].x);
			__m128 b = _mm_loadu_ps(&other.cols[0].x);
			__m128 r = _mm_add_ps(a, b);
			Mat res;
			_mm_storeu_ps(&res.cols[0].x, r);
			return res;
		}
#endif
		Mat res;
		for (size_t c = 0; c < N; ++c) {
			for (size_t r = 0; r < N; ++r) {
				res.cols[c][r] = cols[c][r] + other.cols[c][r];
			}
		}
		return res;
	}

	Mat operator-(const Mat& other) const {
#ifdef GL_USE_SIMD_INTRINSICS
		if constexpr (N == 2) {
			__m128 a = _mm_loadu_ps(&cols[0].x);
			__m128 b = _mm_loadu_ps(&other.cols[0].x);
			__m128 r = _mm_sub_ps(a, b);
			Mat res;
			_mm_storeu_ps(&res.cols[0].x, r);
			return res;
		}
#endif
		Mat res;
		for (size_t c = 0; c < N; ++c) {
			for (size_t r = 0; r < N; ++r) {
				res.cols[c][r] = cols[c][r] - other.cols[c][r];
			}
		}
		return res;
	}

	Mat operator*(const Mat& other) const {
#ifdef GL_USE_SIMD_INTRINSICS
		if constexpr (N == 2) {
			__m128 Col0 =
					_mm_loadl_pi(_mm_setzero_ps(), reinterpret_cast<const __m64*>(&cols[0].x));
			__m128 Col1 =
					_mm_loadl_pi(_mm_setzero_ps(), reinterpret_cast<const __m64*>(&cols[1].x));

			__m128 B_col0 = _mm_loadl_pi(
					_mm_setzero_ps(), reinterpret_cast<const __m64*>(&other.cols[0].x));
			__m128 b00 = _mm_shuffle_ps(B_col0, B_col0, _MM_SHUFFLE(0, 0, 0, 0));
			__m128 b10 = _mm_shuffle_ps(B_col0, B_col0, _MM_SHUFFLE(1, 1, 1, 1));
			__m128 res_col0 = _mm_add_ps(_mm_mul_ps(Col0, b00), _mm_mul_ps(Col1, b10));

			__m128 B_col1 = _mm_loadl_pi(
					_mm_setzero_ps(), reinterpret_cast<const __m64*>(&other.cols[1].x));
			__m128 b01 = _mm_shuffle_ps(B_col1, B_col1, _MM_SHUFFLE(0, 0, 0, 0));
			__m128 b11 = _mm_shuffle_ps(B_col1, B_col1, _MM_SHUFFLE(1, 1, 1, 1));
			__m128 res_col1 = _mm_add_ps(_mm_mul_ps(Col0, b01), _mm_mul_ps(Col1, b11));

			Mat res;
			_mm_storel_pi(reinterpret_cast<__m64*>(&res.cols[0].x), res_col0);
			_mm_storel_pi(reinterpret_cast<__m64*>(&res.cols[1].x), res_col1);
			return res;
		}
#endif
		Mat res = Mat::empty();
		for (size_t c = 0; c < N; ++c) {
			for (size_t r = 0; r < N; ++r) {
				for (size_t k = 0; k < N; ++k) {
					res.cols[c][r] += cols[k][r] * other.cols[c][k];
				}
			}
		}
		return res;
	}

	Vec<N, float> operator*(const Vec<N, float>& v) const {
#ifdef GL_USE_SIMD_INTRINSICS
		if constexpr (N == 2) {
			__m128 Col0 =
					_mm_loadl_pi(_mm_setzero_ps(), reinterpret_cast<const __m64*>(&cols[0].x));
			__m128 Col1 =
					_mm_loadl_pi(_mm_setzero_ps(), reinterpret_cast<const __m64*>(&cols[1].x));

			__m128 vec = _mm_loadl_pi(_mm_setzero_ps(), reinterpret_cast<const __m64*>(&v.x));
			__m128 v0 = _mm_shuffle_ps(vec, vec, _MM_SHUFFLE(0, 0, 0, 0));
			__m128 v1 = _mm_shuffle_ps(vec, vec, _MM_SHUFFLE(1, 1, 1, 1));

			__m128 res_vec = _mm_add_ps(_mm_mul_ps(Col0, v0), _mm_mul_ps(Col1, v1));

			Vec<2, float> res;
			_mm_storel_pi(reinterpret_cast<__m64*>(&res.x), res_vec);
			return res;
		}
#endif
		Vec<N, float> res(0.0f);
		for (size_t r = 0; r < N; ++r) {
			for (size_t c = 0; c < N; ++c) {
				res[r] += cols[c][r] * v[c];
			}
		}
		return res;
	}

	bool operator==(const Mat& other) const {
		for (size_t c = 0; c < N; ++c) {
			for (size_t r = 0; r < N; ++r) {
				if (std::abs(cols[c][r] - other.cols[c][r]) > 1e-6f) {
					return false;
				}
			}
		}
		return true;
	}

	Mat transpose() const {
		Mat res = Mat::empty();
		for (size_t c = 0; c < N; ++c) {
			for (size_t r = 0; r < N; ++r) {
				res.cols[r][c] = cols[c][r];
			}
		}
		return res;
	}
};

template <> struct Mat<3, 3> {
	std::array<Vec3f, 3> cols;

	// Default: Identity matrix
	Mat(float value = 1.0f) :
			cols({
					{ Vec3f{ value, 0.0f, 0.0f } },
					{ Vec3f{ 0.0f, value, 0.0f } },
					{ Vec3f{ 0.0f, 0.0f, value } },
			}) {}

	// Create empty matrix
	static Mat empty() { return Mat{ {} }; }

	Vec3f& operator[](size_t col_idx) { return cols[col_idx]; }
	const Vec3f& operator[](size_t col_idx) const { return cols[col_idx]; }

	Mat operator+(const Mat& other) const {
		Mat res;
#ifdef GL_USE_SIMD_INTRINSICS
		for (int i = 0; i < 3; ++i) {
			__m128 a = load_vec3(&cols[i].x);
			__m128 b = load_vec3(&other.cols[i].x);
			__m128 r = _mm_add_ps(a, b);
			store_vec3(&res.cols[i].x, r);
		}
#else
		for (size_t c = 0; c < 3; ++c) {
			for (size_t r = 0; r < 3; ++r) {
				res.cols[c][r] = cols[c][r] + other.cols[c][r];
			}
		}
#endif
		return res;
	}

	Mat operator-(const Mat& other) const {
		Mat res;
#ifdef GL_USE_SIMD_INTRINSICS
		for (int i = 0; i < 3; ++i) {
			__m128 a = load_vec3(&cols[i].x);
			__m128 b = load_vec3(&other.cols[i].x);
			__m128 r = _mm_sub_ps(a, b);
			store_vec3(&res.cols[i].x, r);
		}
#else
		for (size_t c = 0; c < 3; ++c) {
			for (size_t r = 0; r < 3; ++r) {
				res.cols[c][r] = cols[c][r] - other.cols[c][r];
			}
		}
#endif
		return res;
	}

	Mat operator*(const Mat& other) const {
		Mat res = Mat::empty();
#ifdef GL_USE_SIMD_INTRINSICS
		__m128 Col0 = load_vec3(&cols[0].x);
		__m128 Col1 = load_vec3(&cols[1].x);
		__m128 Col2 = load_vec3(&cols[2].x);

		for (int i = 0; i < 3; ++i) {
			__m128 OtherCol = load_vec3(&other.cols[i].x);

			__m128 e0 = _mm_shuffle_ps(OtherCol, OtherCol, _MM_SHUFFLE(0, 0, 0, 0));
			__m128 e1 = _mm_shuffle_ps(OtherCol, OtherCol, _MM_SHUFFLE(1, 1, 1, 1));
			__m128 e2 = _mm_shuffle_ps(OtherCol, OtherCol, _MM_SHUFFLE(2, 2, 2, 2));

			__m128 m0 = _mm_mul_ps(Col0, e0);
			__m128 m1 = _mm_mul_ps(Col1, e1);
			__m128 m2 = _mm_mul_ps(Col2, e2);

			__m128 sum = _mm_add_ps(_mm_add_ps(m0, m1), m2);

			store_vec3(&res.cols[i].x, sum);
		}
#else
		for (size_t c = 0; c < 3; ++c) {
			for (size_t r = 0; r < 3; ++r) {
				res.cols[c][r] = cols[0][r] * other.cols[c][0] + cols[1][r] * other.cols[c][1] +
						cols[2][r] * other.cols[c][2];
			}
		}
#endif
		return res;
	}

	Vec3f operator*(const Vec3f& v) const {
#ifdef GL_USE_SIMD_INTRINSICS
		__m128 Col0 = load_vec3(&cols[0].x);
		__m128 Col1 = load_vec3(&cols[1].x);
		__m128 Col2 = load_vec3(&cols[2].x);

		__m128 vec = load_vec3(&v.x);

		__m128 v0 = _mm_shuffle_ps(vec, vec, _MM_SHUFFLE(0, 0, 0, 0));
		__m128 v1 = _mm_shuffle_ps(vec, vec, _MM_SHUFFLE(1, 1, 1, 1));
		__m128 v2 = _mm_shuffle_ps(vec, vec, _MM_SHUFFLE(2, 2, 2, 2));

		__m128 m0 = _mm_mul_ps(Col0, v0);
		__m128 m1 = _mm_mul_ps(Col1, v1);
		__m128 m2 = _mm_mul_ps(Col2, v2);

		__m128 res_vec = _mm_add_ps(_mm_add_ps(m0, m1), m2);

		Vec3f res;
		store_vec3(&res.x, res_vec);
		return res;
#else
		Vec3f res;
		res.x = cols[0][0] * v.x + cols[1][0] * v.y + cols[2][0] * v.z;
		res.y = cols[0][1] * v.x + cols[1][1] * v.y + cols[2][1] * v.z;
		res.z = cols[0][2] * v.x + cols[1][2] * v.y + cols[2][2] * v.z;
		return res;
#endif
	}

	bool operator==(const Mat& other) const {
		for (size_t c = 0; c < 3; ++c) {
			for (size_t r = 0; r < 3; ++r) {
				if (std::abs(cols[c][r] - other.cols[c][r]) > 1e-6f) {
					return false;
				}
			}
		}
		return true;
	}

	Mat transpose() const {
		Mat res = Mat::empty();
		for (size_t c = 0; c < 3; ++c) {
			for (size_t r = 0; r < 3; ++r) {
				res.cols[r][c] = cols[c][r];
			}
		}
		return res;
	}

	float determinant() const {
		return cols[0][0] * (cols[1][1] * cols[2][2] - cols[2][1] * cols[1][2]) -
				cols[1][0] * (cols[0][1] * cols[2][2] - cols[2][1] * cols[0][2]) +
				cols[2][0] * (cols[0][1] * cols[1][2] - cols[1][1] * cols[0][2]);
	}

	Mat inverse() const {
		const float det = determinant();
		if (std::abs(det) < 1e-6f) {
			return Mat::empty();
		}

		Mat res = Mat::empty();
		float inv_det = 1.0f / det;

		res.cols[0][0] = (cols[1][1] * cols[2][2] - cols[2][1] * cols[1][2]) * inv_det;
		res.cols[0][1] = -(cols[0][1] * cols[2][2] - cols[2][1] * cols[0][2]) * inv_det;
		res.cols[0][2] = (cols[0][1] * cols[1][2] - cols[1][1] * cols[0][2]) * inv_det;

		res.cols[1][0] = -(cols[1][0] * cols[2][2] - cols[2][0] * cols[1][2]) * inv_det;
		res.cols[1][1] = (cols[0][0] * cols[2][2] - cols[2][0] * cols[0][2]) * inv_det;
		res.cols[1][2] = -(cols[0][0] * cols[1][2] - cols[1][0] * cols[0][2]) * inv_det;

		res.cols[2][0] = (cols[1][0] * cols[2][1] - cols[2][0] * cols[1][1]) * inv_det;
		res.cols[2][1] = -(cols[0][0] * cols[2][1] - cols[2][0] * cols[0][1]) * inv_det;
		res.cols[2][2] = (cols[0][0] * cols[1][1] - cols[1][0] * cols[0][1]) * inv_det;

		return res;
	}
};

template <> struct Mat<4, 4> {
	std::array<Vec4f, 4> cols;

	// Default: Identity matrix
	Mat(float value = 1.0f) :
			cols({
					{ Vec4f{ value, 0.0f, 0.0f, 0.0f } },
					{ Vec4f{ 0.0f, value, 0.0f, 0.0f } },
					{ Vec4f{ 0.0f, 0.0f, value, 0.0f } },
					{ Vec4f{ 0.0f, 0.0f, 0.0f, value } },
			}) {}

	// Create empty matrix
	static Mat empty() { return Mat{ {} }; }

	Vec4f& operator[](size_t col_idx) { return cols[col_idx]; }
	const Vec4f& operator[](size_t col_idx) const { return cols[col_idx]; }

	Mat operator+(const Mat& other) const {
		Mat res;
#ifdef GL_USE_SIMD_INTRINSICS
		// Load, Add, Store for each column
		for (int i = 0; i < 4; ++i) {
			// Unaligned loads are safe and fast on modern CPUs
			__m128 a = _mm_loadu_ps(&cols[i].x);
			__m128 b = _mm_loadu_ps(&other.cols[i].x);
			__m128 r = _mm_add_ps(a, b);
			_mm_storeu_ps(&res.cols[i].x, r);
		}
#else
		for (size_t c = 0; c < 4; ++c) {
			for (size_t r = 0; r < 4; ++r) {
				res.cols[c][r] = cols[c][r] + other.cols[c][r];
			}
		}
#endif
		return res;
	}

	Mat operator-(const Mat& other) const {
		Mat res;
#ifdef GL_USE_SIMD_INTRINSICS
		for (int i = 0; i < 4; ++i) {
			__m128 a = _mm_loadu_ps(&cols[i].x);
			__m128 b = _mm_loadu_ps(&other.cols[i].x);
			__m128 r = _mm_sub_ps(a, b);
			_mm_storeu_ps(&res.cols[i].x, r);
		}
#else
		for (size_t c = 0; c < 4; ++c) {
			for (size_t r = 0; r < 4; ++r) {
				res.cols[c][r] = cols[c][r] - other.cols[c][r];
			}
		}
#endif
		return res;
	}

	Mat operator*(const Mat& other) const {
		Mat res;
#ifdef GL_USE_SIMD_INTRINSICS
		// Load columns of 'this' matrix into registers
		__m128 Col0 = _mm_loadu_ps(&cols[0].x);
		__m128 Col1 = _mm_loadu_ps(&cols[1].x);
		__m128 Col2 = _mm_loadu_ps(&cols[2].x);
		__m128 Col3 = _mm_loadu_ps(&cols[3].x);

		for (int i = 0; i < 4; ++i) {
			// Load one column from the 'other' matrix
			__m128 OtherCol = _mm_loadu_ps(&other.cols[i].x);

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
				res.cols[c][r] = cols[0][r] * other.cols[c][0] + cols[1][r] * other.cols[c][1] +
						cols[2][r] * other.cols[c][2] + cols[3][r] * other.cols[c][3];
			}
		}
#endif
		return res;
	}

	// Matrix * Vector multiplication
	Vec4f operator*(const Vec4f& v) const {
#ifdef GL_USE_SIMD_INTRINSICS
		// Load columns of matrix
		__m128 Col0 = _mm_loadu_ps(&cols[0].x);
		__m128 Col1 = _mm_loadu_ps(&cols[1].x);
		__m128 Col2 = _mm_loadu_ps(&cols[2].x);
		__m128 Col3 = _mm_loadu_ps(&cols[3].x);

		// Load the vector
		__m128 vec = _mm_loadu_ps(&v.x);

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
		res.x = cols[0][0] * v.x + cols[1][0] * v.y + cols[2][0] * v.z + cols[3][0] * v.w;
		res.y = cols[0][1] * v.x + cols[1][1] * v.y + cols[2][1] * v.z + cols[3][1] * v.w;
		res.z = cols[0][2] * v.x + cols[1][2] * v.y + cols[2][2] * v.z + cols[3][2] * v.w;
		res.w = cols[0][3] * v.x + cols[1][3] * v.y + cols[2][3] * v.z + cols[3][3] * v.w;
		return res;
#endif
	}

	bool operator==(const Mat& other) const {
		for (size_t c = 0; c < 4; ++c) {
			for (size_t r = 0; r < 4; ++r) {
				if (std::abs(cols[c][r] - other.cols[c][r]) > 1e-6f) {
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
	float minor(size_t c0, size_t c1, size_t c2, size_t r0, size_t r1, size_t r2) const {
		return cols[c0][r0] * (cols[c1][r1] * cols[c2][r2] - cols[c2][r1] * cols[c1][r2]) -
				cols[c1][r0] * (cols[c0][r1] * cols[c2][r2] - cols[c2][r1] * cols[c0][r2]) +
				cols[c2][r0] * (cols[c0][r1] * cols[c1][r2] - cols[c1][r1] * cols[c0][r2]);
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
	static Mat translate(Vec3f translation) {
		Mat res; // Identity
		res.cols[3][0] = translation.x;
		res.cols[3][1] = translation.y;
		res.cols[3][2] = translation.z;
		return res;
	}

	// Creates a rotation matrix (angle in radians, axis normalized)
	static Mat rotate(float angle_rad, Vec3f axis) {
		Mat res;
		float c = std::cos(angle_rad);
		float s = std::sin(angle_rad);
		float omc = 1.0f - c;

		res.cols[0][0] = axis.x * axis.x * omc + c;
		res.cols[0][1] = axis.y * axis.x * omc + axis.z * s;
		res.cols[0][2] = axis.x * axis.z * omc - axis.y * s;

		res.cols[1][0] = axis.x * axis.y * omc - axis.z * s;
		res.cols[1][1] = axis.y * axis.y * omc + c;
		res.cols[1][2] = axis.y * axis.z * omc + axis.x * s;

		res.cols[2][0] = axis.x * axis.z * omc + axis.y * s;
		res.cols[2][1] = axis.y * axis.z * omc - axis.x * s;
		res.cols[2][2] = axis.z * axis.z * omc + c;

		return res;
	}

	// Turn euler angles to rotation matrix
	static Mat from_euler_angles(const Vec3f& euler_degrees) {
		const float pitch = math::as_radians(euler_degrees.x); // Pitch
		const float yaw = math::as_radians(euler_degrees.y); // Yaw
		const float roll = math::as_radians(euler_degrees.z); // Roll

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
	static Mat scale(Vec3f scale) {
		Mat res;
		res[0][0] = scale.x;
		res[1][1] = scale.y;
		res[2][2] = scale.z;
		return res;
	}

	// Projections

	// LookAt (Right-Handed)
	static Mat look_at(Vec3f eye, Vec3f center, Vec3f up) {
		Vec3f f = (center - eye).normalize(); // Forward
		Vec3f s = f.cross(up).normalize(); // Right
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
		res.cols[3][0] = -s.dot(eye);
		res.cols[3][1] = -u.dot(eye);
		res.cols[3][2] = f.dot(eye);

		return res;
	}

	// Orthographic Projection
	static Mat ortho(float left, float right, float bottom, float top, float z_near, float z_far) {
		Mat res; // Identity
		res.cols[0][0] = 2.0f / (right - left);
		res.cols[1][1] = 2.0f / (top - bottom);
		res.cols[2][2] = -2.0f / (z_far - z_near);

		res.cols[3][0] = -(right + left) / (right - left);
		res.cols[3][1] = -(top + bottom) / (top - bottom);
		res.cols[3][2] = -(z_far + z_near) / (z_far - z_near);
		return res;
	}

	// Perspective Projection (FOV in radians)
	static Mat perspective(float fovy_rad, float aspect, float z_near, float z_far) {
		Mat res(0.0f); // Zero init, not identity

		float const tan_half_fovy = std::tan(fovy_rad / 2.0f);

		res.cols[0][0] = 1.0f / (aspect * tan_half_fovy);
		res.cols[1][1] = 1.0f / (tan_half_fovy);
		res.cols[2][2] = -(z_far + z_near) / (z_far - z_near);
		res.cols[2][3] = -1.0f;
		res.cols[3][2] = -(2.0f * z_far * z_near) / (z_far - z_near);

		return res;
	}
};

typedef Mat<2, 2> Mat2;
typedef Mat<3, 3> Mat3;
typedef Mat<4, 4> Mat4;

}; //namespace gl
