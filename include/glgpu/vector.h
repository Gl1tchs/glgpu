#pragma once

namespace gl {

template <size_t S, typename T> struct Vec;

template <typename T> struct Vec<2, T> {
	T x, y;

	constexpr Vec(T val = static_cast<T>(0)) : x(val), y(val) {}

	constexpr Vec(T x, T y) : x(x), y(y) {}

	constexpr Vec(const Vec& other) = default;

	constexpr Vec(const Vec<3, T>& other) : x(other.x), y(other.y) {}

	constexpr Vec(const Vec<4, T>& other) : x(other.x), y(other.y) {}

	template <typename U>
	constexpr explicit Vec(const Vec<2, U>& other) :
			x(static_cast<T>(other.x)), y(static_cast<T>(other.y)) {}

	template <typename U>
	constexpr Vec(const Vec<3, U>& other) :
			x(static_cast<T>(other.x)), y(static_cast<T>(other.y)) {}

	// Dimension of the vector
	static constexpr size_t size() { return 2; }

	// Directions

	static constexpr Vec zero() { return Vec(0); }
	static constexpr Vec one() { return Vec(1); }

	static constexpr Vec right() { return Vec(1, 0); }
	static constexpr Vec up() { return Vec(0, 1); }

	// Operators

	constexpr T& operator[](size_t col_idx) {
		assert(col_idx == 0 || col_idx == 1);
		return col_idx == 0 ? x : y;
	}
	const T& operator[](size_t col_idx) const {
		assert(col_idx == 0 || col_idx == 1);
		return col_idx == 0 ? x : y;
	}

	constexpr Vec operator-() const { return Vec(-x, -y); }

	constexpr bool operator==(const Vec& rhs) const { return x == rhs.x && y == rhs.y; }

	constexpr T dot(const Vec& other) const { return x * other.x + y * other.y; }

	constexpr T length_sq() const { return dot(*this); }

	constexpr T length() const {
		if constexpr (std::is_floating_point_v<T>) {
			return std::sqrt(length_sq());
		} else {
			return length_sq();
		}
	}

	constexpr Vec normalize() const { return *this / length(); }
};

template <typename T> constexpr Vec<2, T>& operator+=(Vec<2, T>& lhs, const Vec<2, T>& rhs) {
	lhs.x += rhs.x;
	lhs.y += rhs.y;
	return lhs;
}

template <typename T> constexpr Vec<2, T>& operator-=(Vec<2, T>& lhs, const Vec<2, T>& rhs) {
	lhs.x -= rhs.x;
	lhs.y -= rhs.y;
	return lhs;
}

template <typename T> constexpr Vec<2, T>& operator*=(Vec<2, T>& lhs, const T& rhs) {
	lhs.x *= rhs;
	lhs.y *= rhs;
	return lhs;
}

template <typename T> constexpr Vec<2, T>& operator/=(Vec<2, T>& lhs, const T& rhs) {
	if (rhs == 0.0f) {
		lhs = Vec<2, T>(std::numeric_limits<T>::max());
		return lhs;
	}

	lhs.x /= rhs;
	lhs.y /= rhs;
	return lhs;
}

template <typename T> constexpr Vec<2, T> operator+(Vec<2, T> lhs, const Vec<2, T>& rhs) {
	return lhs += rhs;
}

template <typename T> constexpr Vec<2, T> operator-(Vec<2, T> lhs, const Vec<2, T>& rhs) {
	return lhs -= rhs;
}

template <typename T> constexpr Vec<2, T> operator*(Vec<2, T> lhs, const T& rhs) {
	return lhs *= rhs;
}

template <typename T> constexpr Vec<2, T> operator/(Vec<2, T> lhs, const T& rhs) {
	return lhs /= rhs;
}

using Vec2f = Vec<2, float>;
using Vec2d = Vec<2, double>;
using Vec2i = Vec<2, int>;
using Vec2u = Vec<2, uint32_t>;

template <typename T> struct Vec<3, T> {
	T x, y, z;

	constexpr Vec(T val = static_cast<T>(0)) : x(val), y(val), z(val) {}

	constexpr Vec(T x, T y, T z) : x(x), y(y), z(z) {}

	constexpr Vec(const Vec& other) = default;

	constexpr Vec(const Vec<2, T>& other, T z = static_cast<T>(0)) : x(other.x), y(other.y), z(z) {}

	constexpr Vec(const Vec<4, T>& other) : x(other.x), y(other.y), z(other.z) {}

	template <typename U>
	constexpr explicit Vec(const Vec<3, U>& other) :
			x(static_cast<T>(other.x)), y(static_cast<T>(other.y)), z(static_cast<T>(other.z)) {}

	template <typename U>
	constexpr explicit Vec(const Vec<2, U>& other, U z = static_cast<U>(0)) :
			x(static_cast<T>(other.x)), y(static_cast<T>(other.y)), z(static_cast<U>(z)) {}

	// Dimension of the vector
	static constexpr size_t size() { return 3; }

	// Directions

	static constexpr Vec zero() { return Vec(0); }
	static constexpr Vec one() { return Vec(1); }

	static constexpr Vec right() { return Vec(1, 0, 0); }
	static constexpr Vec up() { return Vec(0, 1, 0); }
	static constexpr Vec forward() { return Vec(0, 0, -1); }

	// Operators

	constexpr T& operator[](size_t col_idx) {
		assert(col_idx >= 0 || col_idx <= 2);
		return col_idx == 0 ? x : col_idx == 1 ? y : z;
	}
	const T& operator[](size_t col_idx) const {
		assert(col_idx >= 0 || col_idx <= 2);
		return col_idx == 0 ? x : col_idx == 1 ? y : z;
	}

	constexpr Vec operator-() const { return Vec(-x, -y, -z); }

	constexpr bool operator==(const Vec& rhs) const {
		return x == rhs.x && y == rhs.y && z == rhs.z;
	}

	// Methods

	constexpr T dot(const Vec& other) const { return x * other.x + y * other.y + z * other.z; }

	constexpr Vec cross(const Vec& other) const {
		return Vec(y * other.z - z * other.y, z * other.x - x * other.z, x * other.y - y * other.x);
	}

	constexpr T length_sq() const { return dot(*this); }

	constexpr T length() const {
		if constexpr (std::is_floating_point_v<T>) {
			return std::sqrt(length_sq());
		} else {
			return length_sq();
		}
	}

	constexpr Vec normalize() const { return *this / length(); }
};

template <typename T> constexpr Vec<3, T>& operator+=(Vec<3, T>& lhs, const Vec<3, T>& rhs) {
	lhs.x += rhs.x;
	lhs.y += rhs.y;
	lhs.z += rhs.z;
	return lhs;
}

template <typename T> constexpr Vec<3, T>& operator-=(Vec<3, T>& lhs, const Vec<3, T>& rhs) {
	lhs.x -= rhs.x;
	lhs.y -= rhs.y;
	lhs.z -= rhs.z;
	return lhs;
}

template <typename T> constexpr Vec<3, T>& operator*=(Vec<3, T>& lhs, const T& rhs) {
	lhs.x *= rhs;
	lhs.y *= rhs;
	lhs.z *= rhs;
	return lhs;
}

template <typename T> constexpr Vec<3, T>& operator/=(Vec<3, T>& lhs, const T& rhs) {
	if (rhs == 0.0f) {
		lhs = Vec<3, T>(std::numeric_limits<T>::max());
		return lhs;
	}

	lhs.x /= rhs;
	lhs.y /= rhs;
	lhs.z /= rhs;
	return lhs;
}

template <typename T> constexpr Vec<3, T> operator+(Vec<3, T> lhs, const Vec<3, T>& rhs) {
	return lhs += rhs;
}

template <typename T> constexpr Vec<3, T> operator-(Vec<3, T> lhs, const Vec<3, T>& rhs) {
	return lhs -= rhs;
}

template <typename T> constexpr Vec<3, T> operator*(Vec<3, T> lhs, const T& rhs) {
	return lhs *= rhs;
}

template <typename T> constexpr Vec<3, T> operator/(Vec<3, T> lhs, const T& rhs) {
	return lhs /= rhs;
}

using Vec3f = Vec<3, float>;
using Vec3d = Vec<3, double>;
using Vec3i = Vec<3, int>;
using Vec3u = Vec<3, uint32_t>;

template <typename T> struct Vec<4, T> {
	T x, y, z, w;

	constexpr Vec(T val = 0.0f) : x(val), y(val), z(val), w(val) {}

	constexpr Vec(T x, T y, T z, T w) : x(x), y(y), z(z), w(w) {}

	constexpr Vec(const Vec& other) = default;

	constexpr Vec(const Vec<3, T>& v, T w = static_cast<T>(0)) : x(v.x), y(v.y), z(v.z), w(w) {}

	template <typename U>
	constexpr explicit Vec(const Vec<4, U>& other) :
			x(static_cast<T>(other.x)),
			y(static_cast<T>(other.y)),
			z(static_cast<T>(other.z)),
			w(static_cast<T>(other.w)) {}

	// Dimension of the vector
	static constexpr size_t size() { return 4; }

	// Directions

	static constexpr Vec zero() { return Vec(0); }
	static constexpr Vec one() { return Vec(1); }

	static constexpr Vec right() { return Vec(1, 0, 0, 0); }
	static constexpr Vec up() { return Vec(0, 1, 0, 0); }
	static constexpr Vec forward() { return Vec(0, 0, -1, 0); }

	// Operators

	constexpr T& operator[](size_t col_idx) {
		assert(col_idx >= 0 || col_idx <= 3);
		return col_idx == 0 ? x : col_idx == 1 ? y : col_idx == 2 ? z : w;
	}
	const T& operator[](size_t col_idx) const {
		assert(col_idx >= 0 || col_idx <= 3);
		return col_idx == 0 ? x : col_idx == 1 ? y : col_idx == 2 ? z : w;
	}

	constexpr Vec operator-() const { return Vec(-x, -y, -z, -w); }

	constexpr bool operator==(const Vec& rhs) const {
		return x == rhs.x && y == rhs.y && z == rhs.z && w == rhs.w;
	}

	constexpr T dot(const Vec& other) const {
		return x * other.x + y * other.y + z * other.z + w * other.w;
	}

	constexpr T length_sq() const { return dot(*this); }

	constexpr T length() const {
		if constexpr (std::is_floating_point_v<T>) {
			return std::sqrt(length_sq());
		} else {
			return length_sq();
		}
	}

	constexpr Vec normalize() const { return *this / length(); }
};

template <typename T> constexpr Vec<4, T>& operator+=(Vec<4, T>& lhs, const Vec<4, T>& rhs) {
	lhs.x += rhs.x;
	lhs.y += rhs.y;
	lhs.z += rhs.z;
	lhs.w += rhs.w;
	return lhs;
}

template <typename T> constexpr Vec<4, T>& operator-=(Vec<4, T>& lhs, const Vec<4, T>& rhs) {
	lhs.x -= rhs.x;
	lhs.y -= rhs.y;
	lhs.z -= rhs.z;
	lhs.w -= rhs.w;
	return lhs;
}

template <typename T> constexpr Vec<4, T>& operator*=(Vec<4, T>& lhs, const T& rhs) {
	lhs.x *= rhs;
	lhs.y *= rhs;
	lhs.z *= rhs;
	lhs.w *= rhs;
	return lhs;
}

template <typename T> constexpr Vec<4, T>& operator/=(Vec<4, T>& lhs, const T& rhs) {
	if (rhs == 0.0f) {
		lhs = Vec<4, T>(std::numeric_limits<T>::max());
		return lhs;
	}

	lhs.x /= rhs;
	lhs.y /= rhs;
	lhs.z /= rhs;
	lhs.w /= rhs;
	return lhs;
}

template <typename T> constexpr Vec<4, T> operator+(Vec<4, T> lhs, const Vec<4, T>& rhs) {
	return lhs += rhs;
}

template <typename T> constexpr Vec<4, T> operator-(Vec<4, T> lhs, const Vec<4, T>& rhs) {
	return lhs -= rhs;
}

template <typename T> constexpr Vec<4, T> operator*(Vec<4, T> lhs, const T& rhs) {
	return lhs *= rhs;
}

template <typename T> constexpr Vec<4, T> operator/(Vec<4, T> lhs, const T& rhs) {
	return lhs /= rhs;
}

using Vec4f = Vec<4, float>;
using Vec4d = Vec<4, double>;
using Vec4i = Vec<4, int>;
using Vec4u = Vec<4, uint32_t>;

} // namespace gl
