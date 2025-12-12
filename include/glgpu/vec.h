#pragma once

namespace gl {

template <typename T> struct Vec3;

template <typename T> struct Vec2 {
	T x, y;

	Vec2() = default;

	explicit Vec2(T val) : x(val), y(val) {}

	Vec2(T x_val, T y_val) : x(x_val), y(y_val) {}

	Vec2(const Vec2<T>& other) : x(other.x), y(other.y) {}

	Vec2(const Vec3<T>& other) : x(other.x), y(other.y) {}

	template <typename U>
	explicit Vec2(const Vec2<U>& other) : x(static_cast<T>(other.x)), y(static_cast<T>(other.y)) {}

	template <typename U>
	Vec2(const Vec3<U>& other) : x(static_cast<T>(other.x)), y(static_cast<T>(other.y)) {}

	/**
	 * @brief Computes the dot product of this vector and another.
	 * @param other The other vector.
	 * @return The dot product result.
	 */
	T dot(const Vec2<T>& other) const { return x * other.x + y * other.y; }

	/**
	 * @brief Computes the squared length (magnitude) of the vector.
	 * @return The squared length.
	 */
	T length_sq() const { return dot(*this); }

	/**
	 * @brief Computes the length (magnitude) of the vector.
	 * @return The length.
	 */
	T length() const {
		// Only safe to use std::sqrt if T is a floating-point type
		if constexpr (std::is_floating_point_v<T>) {
			return std::sqrt(length_sq());
		} else {
			// For integer types, length is typically only useful as an approximation or for
			// comparison
			return length_sq();
		}
	}
};

typedef Vec2<float> Vec2f;
typedef Vec2<double> Vec2d;
typedef Vec2<int> Vec2i;
typedef Vec2<uint32_t> Vec2u;

template <typename T> struct Vec3 {
	T x, y, z;

	Vec3() = default;

	explicit Vec3(T val) : x(val), y(val), z(val) {}

	Vec3(T x_val, T y_val, T z_val) : x(x_val), y(y_val), z(z_val) {}

	Vec3(const Vec3<T>& other) : x(other.x), y(other.y), z(other.z) {}

	template <typename U>
	explicit Vec3(const Vec3<U>& other) :
			x(static_cast<T>(other.x)), y(static_cast<T>(other.y)), z(static_cast<T>(other.z)) {}

	template <typename U>
	explicit Vec3(const Vec2<U>& other) :
			x(static_cast<T>(other.x)), y(static_cast<T>(other.y)), z(static_cast<T>(0)) {}

	template <typename U>
	Vec3(const Vec2<U>& xy, T z_val) : x(static_cast<T>(xy.x)), y(static_cast<T>(xy.y)), z(z_val) {}

	/**
	 * @brief Computes the dot product of this vector and another.
	 * @param other The other vector.
	 * @return The dot product result.
	 */
	T dot(const Vec3<T>& other) const { return x * other.x + y * other.y + z * other.z; }

	/**
	 * @brief Computes the cross product of this vector and another.
	 * @param other The other vector.
	 * @return The resulting vector perpendicular to both.
	 */
	Vec3<T> cross(const Vec3<T>& other) const {
		return Vec3<T>(
				y * other.z - z * other.y, z * other.x - x * other.z, x * other.y - y * other.x);
	}

	/**
	 * @brief Computes the squared length (magnitude) of the vector.
	 * @return The squared length.
	 */
	T length_sq() const { return dot(*this); }

	/**
	 * @brief Computes the length (magnitude) of the vector.
	 * @return The length.
	 */
	T length() const {
		// Only safe to use std::sqrt if T is a floating-point type
		if constexpr (std::is_floating_point_v<T>) {
			return std::sqrt(length_sq());
		} else {
			// For integer types, length is typically only useful as an approximation or for
			// comparison
			return length_sq();
		}
	}
};

typedef Vec3<float> Vec3f;
typedef Vec3<double> Vec3d;
typedef Vec3<int> Vec3i;
typedef Vec3<uint32_t> Vec3u;

template <typename T> struct Vec4 {
	T x, y, z, w;
};

typedef Vec4<float> Vec4f;
typedef Vec4<double> Vec4d;
typedef Vec4<int> Vec4i;
typedef Vec4<uint32_t> Vec4u;

} //namespace gl
