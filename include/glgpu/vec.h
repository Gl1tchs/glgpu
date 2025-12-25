#pragma once

#include <limits>
namespace gl {

template <typename T> struct Vec2;
template <typename T> struct Vec3;
template <typename T> struct Vec4;

template <typename T> struct Vec2 {
	T x, y;

	constexpr Vec2(T p_val = 0.0f) : x(p_val), y(p_val) {}

	constexpr Vec2(T p_x, T p_y) : x(p_x), y(p_y) {}

	constexpr Vec2(const Vec2& p_other) = default;

	Vec2(const Vec3<T>& p_other);

	Vec2(const Vec4<T>& p_other);

	template <typename U>
	constexpr explicit Vec2(const Vec2<U>& p_other) :
			x(static_cast<T>(p_other.x)), y(static_cast<T>(p_other.y)) {}

	template <typename U> Vec2(const Vec3<U>& p_other); // Defined below

	constexpr bool operator==(const Vec2& p_rhs) const { return x == p_rhs.x && y == p_rhs.y; }

	constexpr T dot(const Vec2& p_other) const { return x * p_other.x + y * p_other.y; }

	constexpr T length_sq() const { return dot(*this); }

	constexpr T length() const {
		if constexpr (std::is_floating_point_v<T>) {
			return std::sqrt(length_sq());
		} else {
			return length_sq();
		}
	}

	constexpr Vec2 normalize() const { return *this / length(); }
};

template <typename T> constexpr Vec2<T>& operator+=(Vec2<T>& p_lhs, const Vec2<T>& p_rhs) {
	p_lhs.x += p_rhs.x;
	p_lhs.y += p_rhs.y;
	return p_lhs;
}

template <typename T> constexpr Vec2<T>& operator-=(Vec2<T>& p_lhs, const Vec2<T>& p_rhs) {
	p_lhs.x -= p_rhs.x;
	p_lhs.y -= p_rhs.y;
	return p_lhs;
}

template <typename T> constexpr Vec2<T>& operator*=(Vec2<T>& p_lhs, const T& p_rhs) {
	p_lhs.x *= p_rhs;
	p_lhs.y *= p_rhs;
	return p_lhs;
}

template <typename T> constexpr Vec2<T>& operator/=(Vec2<T>& p_lhs, const T& p_rhs) {
	if (p_rhs == 0.0f) {
		p_lhs = Vec2(std::numeric_limits<T>::max());
		return p_lhs;
	}

	p_lhs.x /= p_rhs;
	p_lhs.y /= p_rhs;
	return p_lhs;
}

template <typename T> constexpr Vec2<T> operator+(Vec2<T> p_lhs, const Vec2<T>& p_rhs) {
	return p_lhs += p_rhs;
}

template <typename T> constexpr Vec2<T> operator-(Vec2<T> p_lhs, const Vec2<T>& p_rhs) {
	return p_lhs -= p_rhs;
}

template <typename T> constexpr Vec2<T> operator*(Vec2<T> p_lhs, const T& p_rhs) {
	return p_lhs *= p_rhs;
}

template <typename T> constexpr Vec2<T> operator/(Vec2<T> p_lhs, const T& p_rhs) {
	return p_lhs /= p_rhs;
}

using Vec2f = Vec2<float>;
using Vec2d = Vec2<double>;
using Vec2i = Vec2<int>;
using Vec2u = Vec2<uint32_t>;

template <typename T> struct Vec3 {
	T x, y, z;

	constexpr Vec3(T p_val = 0.0f) : x(p_val), y(p_val), z(p_val) {}

	constexpr Vec3(T p_x, T p_y, T p_z) : x(p_x), y(p_y), z(p_z) {}

	constexpr Vec3(const Vec3& p_other) = default;

	Vec3(const Vec2<T>& p_other, T p_z_val = static_cast<T>(0));

	Vec3(const Vec4<T>& p_other);

	template <typename U>
	constexpr explicit Vec3(const Vec3<U>& p_other) :
			x(static_cast<T>(p_other.x)),
			y(static_cast<T>(p_other.y)),
			z(static_cast<T>(p_other.z)) {}

	template <typename U>
	explicit Vec3(const Vec2<U>& p_other, T p_z_val = static_cast<T>(0)); // Defined below

	// Operators

	constexpr bool operator==(const Vec3& p_rhs) const {
		return x == p_rhs.x && y == p_rhs.y && z == p_rhs.z;
	}

	// Methods

	constexpr T dot(const Vec3& p_other) const {
		return x * p_other.x + y * p_other.y + z * p_other.z;
	}

	constexpr Vec3 cross(const Vec3& p_other) const {
		return Vec3(y * p_other.z - z * p_other.y, z * p_other.x - x * p_other.z,
				x * p_other.y - y * p_other.x);
	}

	constexpr T length_sq() const { return dot(*this); }

	constexpr T length() const {
		if constexpr (std::is_floating_point_v<T>) {
			return std::sqrt(length_sq());
		} else {
			return length_sq();
		}
	}

	constexpr Vec3 normalize() const { return *this / length(); }
};

template <typename T> constexpr Vec3<T>& operator+=(Vec3<T>& p_lhs, const Vec3<T>& p_rhs) {
	p_lhs.x += p_rhs.x;
	p_lhs.y += p_rhs.y;
	p_lhs.z += p_rhs.z;
	return p_lhs;
}

template <typename T> constexpr Vec3<T>& operator-=(Vec3<T>& p_lhs, const Vec3<T>& p_rhs) {
	p_lhs.x -= p_rhs.x;
	p_lhs.y -= p_rhs.y;
	p_lhs.z -= p_rhs.z;
	return p_lhs;
}

template <typename T> constexpr Vec3<T>& operator*=(Vec3<T>& p_lhs, const T& p_rhs) {
	p_lhs.x *= p_rhs;
	p_lhs.y *= p_rhs;
	p_lhs.z *= p_rhs;
	return p_lhs;
}

template <typename T> constexpr Vec3<T>& operator/=(Vec3<T>& p_lhs, const T& p_rhs) {
	if (p_rhs == 0.0f) {
		p_lhs = Vec3(std::numeric_limits<T>::max());
		return p_lhs;
	}

	p_lhs.x /= p_rhs;
	p_lhs.y /= p_rhs;
	p_lhs.z /= p_rhs;
	return p_lhs;
}

template <typename T> constexpr Vec3<T> operator+(Vec3<T> p_lhs, const Vec3<T>& p_rhs) {
	return p_lhs += p_rhs;
}

template <typename T> constexpr Vec3<T> operator-(Vec3<T> p_lhs, const Vec3<T>& p_rhs) {
	return p_lhs -= p_rhs;
}

template <typename T> constexpr Vec3<T> operator*(Vec3<T> p_lhs, const T& p_rhs) {
	return p_lhs *= p_rhs;
}

template <typename T> constexpr Vec3<T> operator/(Vec3<T> p_lhs, const T& p_rhs) {
	return p_lhs /= p_rhs;
}

using Vec3f = Vec3<float>;
using Vec3d = Vec3<double>;
using Vec3i = Vec3<int>;
using Vec3u = Vec3<uint32_t>;

template <typename T> struct Vec4 {
	T x, y, z, w;

	constexpr Vec4(T p_val = 0.0f) : x(p_val), y(p_val), z(p_val), w(p_val) {}

	constexpr Vec4(T p_x, T p_y, T p_z, T p_w) : x(p_x), y(p_y), z(p_z), w(p_w) {}

	constexpr Vec4(const Vec4& p_other) = default;

	Vec4(const Vec3<T>& v, T w_val = static_cast<T>(1));

	template <typename U>
	constexpr explicit Vec4(const Vec4<U>& p_other) :
			x(static_cast<T>(p_other.x)),
			y(static_cast<T>(p_other.y)),
			z(static_cast<T>(p_other.z)),
			w(static_cast<T>(p_other.w)) {}

	constexpr bool operator==(const Vec4& p_rhs) const {
		return x == p_rhs.x && y == p_rhs.y && z == p_rhs.z && w == p_rhs.w;
	}

	constexpr T dot(const Vec4& p_other) const {
		return x * p_other.x + y * p_other.y + z * p_other.z + w * p_other.w;
	}

	constexpr T length_sq() const { return dot(*this); }

	constexpr T length() const {
		if constexpr (std::is_floating_point_v<T>) {
			return std::sqrt(length_sq());
		} else {
			return length_sq();
		}
	}

	constexpr Vec4 normalize() const { return *this / length(); }
};

template <typename T> constexpr Vec4<T>& operator+=(Vec4<T>& p_lhs, const Vec4<T>& p_rhs) {
	p_lhs.x += p_rhs.x;
	p_lhs.y += p_rhs.y;
	p_lhs.z += p_rhs.z;
	p_lhs.w += p_rhs.w;
	return p_lhs;
}

template <typename T> constexpr Vec4<T>& operator-=(Vec4<T>& p_lhs, const Vec4<T>& p_rhs) {
	p_lhs.x -= p_rhs.x;
	p_lhs.y -= p_rhs.y;
	p_lhs.z -= p_rhs.z;
	p_lhs.w -= p_rhs.w;
	return p_lhs;
}

template <typename T> constexpr Vec4<T>& operator*=(Vec4<T>& p_lhs, const T& p_rhs) {
	p_lhs.x *= p_rhs;
	p_lhs.y *= p_rhs;
	p_lhs.z *= p_rhs;
	p_lhs.w *= p_rhs;
	return p_lhs;
}

template <typename T> constexpr Vec4<T>& operator/=(Vec4<T>& p_lhs, const T& p_rhs) {
	if (p_rhs == 0.0f) {
		p_lhs = Vec4(std::numeric_limits<T>::max());
		return p_lhs;
	}

	p_lhs.x /= p_rhs;
	p_lhs.y /= p_rhs;
	p_lhs.z /= p_rhs;
	p_lhs.w /= p_rhs;
	return p_lhs;
}

template <typename T> constexpr Vec4<T> operator+(Vec4<T> p_lhs, const Vec4<T>& p_rhs) {
	return p_lhs += p_rhs;
}

template <typename T> constexpr Vec4<T> operator-(Vec4<T> p_lhs, const Vec4<T>& p_rhs) {
	return p_lhs -= p_rhs;
}

template <typename T> constexpr Vec4<T> operator*(Vec4<T> p_lhs, const T& p_rhs) {
	return p_lhs *= p_rhs;
}

template <typename T> constexpr Vec4<T> operator/(Vec4<T> p_lhs, const T& p_rhs) {
	return p_lhs /= p_rhs;
}

using Vec4f = Vec4<float>;
using Vec4d = Vec4<double>;
using Vec4i = Vec4<int>;
using Vec4u = Vec4<uint32_t>;

template <typename T> Vec2<T>::Vec2(const Vec3<T>& p_other) : x(p_other.x), y(p_other.y) {}

template <typename T> Vec2<T>::Vec2(const Vec4<T>& p_other) : x(p_other.x), y(p_other.y) {}

template <typename T>
template <typename U>
Vec2<T>::Vec2(const Vec3<U>& p_other) :
		x(static_cast<T>(p_other.x)), y(static_cast<T>(p_other.y)) {}

template <typename T>
Vec3<T>::Vec3(const Vec2<T>& p_other, T p_z) : x(p_other.x), y(p_other.y), z(p_z) {}

template <typename T>
Vec3<T>::Vec3(const Vec4<T>& p_other) : x(p_other.x), y(p_other.y), z(p_other.z) {}

template <typename T>
template <typename U>
Vec3<T>::Vec3(const Vec2<U>& p_other, T p_z) :
		x(static_cast<T>(p_other.x)), y(static_cast<T>(p_other.y)), z(p_z) {}

template <typename T>
Vec4<T>::Vec4(const Vec3<T>& p_v, T p_w) : x(p_v.x), y(p_v.y), z(p_v.z), w(p_w) {}

} // namespace gl
