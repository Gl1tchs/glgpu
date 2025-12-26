#pragma once

namespace gl {

template <size_t S, typename T> struct Vec;

template <typename T> struct Vec<2, T> {
	T x, y;

	constexpr Vec(T p_val = static_cast<T>(0)) : x(p_val), y(p_val) {}

	constexpr Vec(T p_x, T p_y) : x(p_x), y(p_y) {}

	constexpr Vec(const Vec& p_other) = default;

	constexpr Vec(const Vec<3, T>& p_other) : x(p_other.x), y(p_other.y) {}

	constexpr Vec(const Vec<4, T>& p_other) : x(p_other.x), y(p_other.y) {}

	template <typename U>
	constexpr explicit Vec(const Vec<2, U>& p_other) :
			x(static_cast<T>(p_other.x)), y(static_cast<T>(p_other.y)) {}

	template <typename U>
	constexpr Vec(const Vec<3, U>& p_other) :
			x(static_cast<T>(p_other.x)), y(static_cast<T>(p_other.y)) {}

	// Dimension of the vector
	static constexpr size_t size() { return 2; }

	// Directions

	static constexpr Vec zero() { return Vec(0); }
	static constexpr Vec one() { return Vec(1); }

	static constexpr Vec right() { return Vec(1, 0); }
	static constexpr Vec up() { return Vec(0, 1); }

	// Operators

	constexpr T& operator[](size_t p_col_idx) {
		assert(p_col_idx == 0 || p_col_idx == 1);
		return p_col_idx == 0 ? x : y;
	}
	const T& operator[](size_t p_col_idx) const {
		assert(p_col_idx == 0 || p_col_idx == 1);
		return p_col_idx == 0 ? x : y;
	}

	constexpr Vec operator-() const { return Vec(-x, -y); }

	constexpr bool operator==(const Vec& p_rhs) const { return x == p_rhs.x && y == p_rhs.y; }

	constexpr T dot(const Vec& p_other) const { return x * p_other.x + y * p_other.y; }

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

template <typename T> constexpr Vec<2, T>& operator+=(Vec<2, T>& p_lhs, const Vec<2, T>& p_rhs) {
	p_lhs.x += p_rhs.x;
	p_lhs.y += p_rhs.y;
	return p_lhs;
}

template <typename T> constexpr Vec<2, T>& operator-=(Vec<2, T>& p_lhs, const Vec<2, T>& p_rhs) {
	p_lhs.x -= p_rhs.x;
	p_lhs.y -= p_rhs.y;
	return p_lhs;
}

template <typename T> constexpr Vec<2, T>& operator*=(Vec<2, T>& p_lhs, const T& p_rhs) {
	p_lhs.x *= p_rhs;
	p_lhs.y *= p_rhs;
	return p_lhs;
}

template <typename T> constexpr Vec<2, T>& operator/=(Vec<2, T>& p_lhs, const T& p_rhs) {
	if (p_rhs == 0.0f) {
		p_lhs = Vec<2, T>(std::numeric_limits<T>::max());
		return p_lhs;
	}

	p_lhs.x /= p_rhs;
	p_lhs.y /= p_rhs;
	return p_lhs;
}

template <typename T> constexpr Vec<2, T> operator+(Vec<2, T> p_lhs, const Vec<2, T>& p_rhs) {
	return p_lhs += p_rhs;
}

template <typename T> constexpr Vec<2, T> operator-(Vec<2, T> p_lhs, const Vec<2, T>& p_rhs) {
	return p_lhs -= p_rhs;
}

template <typename T> constexpr Vec<2, T> operator*(Vec<2, T> p_lhs, const T& p_rhs) {
	return p_lhs *= p_rhs;
}

template <typename T> constexpr Vec<2, T> operator/(Vec<2, T> p_lhs, const T& p_rhs) {
	return p_lhs /= p_rhs;
}

using Vec2f = Vec<2, float>;
using Vec2d = Vec<2, double>;
using Vec2i = Vec<2, int>;
using Vec2u = Vec<2, uint32_t>;

template <typename T> struct Vec<3, T> {
	T x, y, z;

	constexpr Vec(T p_val = static_cast<T>(0)) : x(p_val), y(p_val), z(p_val) {}

	constexpr Vec(T p_x, T p_y, T p_z) : x(p_x), y(p_y), z(p_z) {}

	constexpr Vec(const Vec& p_other) = default;

	constexpr Vec(const Vec<2, T>& p_other, T p_z = static_cast<T>(0)) :
			x(p_other.x), y(p_other.y), z(p_z) {}

	constexpr Vec(const Vec<4, T>& p_other) : x(p_other.x), y(p_other.y), z(p_other.z) {}

	template <typename U>
	constexpr explicit Vec(const Vec<3, U>& p_other) :
			x(static_cast<T>(p_other.x)),
			y(static_cast<T>(p_other.y)),
			z(static_cast<T>(p_other.z)) {}

	template <typename U>
	constexpr explicit Vec(const Vec<2, U>& p_other, U p_z = static_cast<U>(0)) :
			x(static_cast<T>(p_other.x)), y(static_cast<T>(p_other.y)), z(static_cast<U>(p_z)) {}

	// Dimension of the vector
	static constexpr size_t size() { return 3; }

	// Directions

	static constexpr Vec zero() { return Vec(0); }
	static constexpr Vec one() { return Vec(1); }

	static constexpr Vec right() { return Vec(1, 0, 0); }
	static constexpr Vec up() { return Vec(0, 1, 0); }
	static constexpr Vec forward() { return Vec(0, 0, -1); }

	// Operators

	constexpr T& operator[](size_t p_col_idx) {
		assert(p_col_idx >= 0 || p_col_idx <= 2);
		return p_col_idx == 0 ? x : p_col_idx == 1 ? y : z;
	}
	const T& operator[](size_t p_col_idx) const {
		assert(p_col_idx >= 0 || p_col_idx <= 2);
		return p_col_idx == 0 ? x : p_col_idx == 1 ? y : z;
	}

	constexpr Vec operator-() const { return Vec(-x, -y, -z); }

	constexpr bool operator==(const Vec& p_rhs) const {
		return x == p_rhs.x && y == p_rhs.y && z == p_rhs.z;
	}

	// Methods

	constexpr T dot(const Vec& p_other) const {
		return x * p_other.x + y * p_other.y + z * p_other.z;
	}

	constexpr Vec cross(const Vec& p_other) const {
		return Vec(y * p_other.z - z * p_other.y, z * p_other.x - x * p_other.z,
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

	constexpr Vec normalize() const { return *this / length(); }
};

template <typename T> constexpr Vec<3, T>& operator+=(Vec<3, T>& p_lhs, const Vec<3, T>& p_rhs) {
	p_lhs.x += p_rhs.x;
	p_lhs.y += p_rhs.y;
	p_lhs.z += p_rhs.z;
	return p_lhs;
}

template <typename T> constexpr Vec<3, T>& operator-=(Vec<3, T>& p_lhs, const Vec<3, T>& p_rhs) {
	p_lhs.x -= p_rhs.x;
	p_lhs.y -= p_rhs.y;
	p_lhs.z -= p_rhs.z;
	return p_lhs;
}

template <typename T> constexpr Vec<3, T>& operator*=(Vec<3, T>& p_lhs, const T& p_rhs) {
	p_lhs.x *= p_rhs;
	p_lhs.y *= p_rhs;
	p_lhs.z *= p_rhs;
	return p_lhs;
}

template <typename T> constexpr Vec<3, T>& operator/=(Vec<3, T>& p_lhs, const T& p_rhs) {
	if (p_rhs == 0.0f) {
		p_lhs = Vec<3, T>(std::numeric_limits<T>::max());
		return p_lhs;
	}

	p_lhs.x /= p_rhs;
	p_lhs.y /= p_rhs;
	p_lhs.z /= p_rhs;
	return p_lhs;
}

template <typename T> constexpr Vec<3, T> operator+(Vec<3, T> p_lhs, const Vec<3, T>& p_rhs) {
	return p_lhs += p_rhs;
}

template <typename T> constexpr Vec<3, T> operator-(Vec<3, T> p_lhs, const Vec<3, T>& p_rhs) {
	return p_lhs -= p_rhs;
}

template <typename T> constexpr Vec<3, T> operator*(Vec<3, T> p_lhs, const T& p_rhs) {
	return p_lhs *= p_rhs;
}

template <typename T> constexpr Vec<3, T> operator/(Vec<3, T> p_lhs, const T& p_rhs) {
	return p_lhs /= p_rhs;
}

using Vec3f = Vec<3, float>;
using Vec3d = Vec<3, double>;
using Vec3i = Vec<3, int>;
using Vec3u = Vec<3, uint32_t>;

template <typename T> struct Vec<4, T> {
	T x, y, z, w;

	constexpr Vec(T p_val = 0.0f) : x(p_val), y(p_val), z(p_val), w(p_val) {}

	constexpr Vec(T p_x, T p_y, T p_z, T p_w) : x(p_x), y(p_y), z(p_z), w(p_w) {}

	constexpr Vec(const Vec& p_other) = default;

	constexpr Vec(const Vec<3, T>& p_v, T p_w = static_cast<T>(0)) :
			x(p_v.x), y(p_v.y), z(p_v.z), w(p_w) {}

	template <typename U>
	constexpr explicit Vec(const Vec<4, U>& p_other) :
			x(static_cast<T>(p_other.x)),
			y(static_cast<T>(p_other.y)),
			z(static_cast<T>(p_other.z)),
			w(static_cast<T>(p_other.w)) {}

	// Dimension of the vector
	static constexpr size_t size() { return 4; }

	// Directions

	static constexpr Vec zero() { return Vec(0); }
	static constexpr Vec one() { return Vec(1); }

	static constexpr Vec right() { return Vec(1, 0, 0, 0); }
	static constexpr Vec up() { return Vec(0, 1, 0, 0); }
	static constexpr Vec forward() { return Vec(0, 0, -1, 0); }

	// Operators

	constexpr T& operator[](size_t p_col_idx) {
		assert(p_col_idx >= 0 || p_col_idx <= 3);
		return p_col_idx == 0 ? x : p_col_idx == 1 ? y : p_col_idx == 2 ? z : w;
	}
	const T& operator[](size_t p_col_idx) const {
		assert(p_col_idx >= 0 || p_col_idx <= 3);
		return p_col_idx == 0 ? x : p_col_idx == 1 ? y : p_col_idx == 2 ? z : w;
	}

	constexpr Vec operator-() const { return Vec(-x, -y, -z, -w); }

	constexpr bool operator==(const Vec& p_rhs) const {
		return x == p_rhs.x && y == p_rhs.y && z == p_rhs.z && w == p_rhs.w;
	}

	constexpr T dot(const Vec& p_other) const {
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

	constexpr Vec normalize() const { return *this / length(); }
};

template <typename T> constexpr Vec<4, T>& operator+=(Vec<4, T>& p_lhs, const Vec<4, T>& p_rhs) {
	p_lhs.x += p_rhs.x;
	p_lhs.y += p_rhs.y;
	p_lhs.z += p_rhs.z;
	p_lhs.w += p_rhs.w;
	return p_lhs;
}

template <typename T> constexpr Vec<4, T>& operator-=(Vec<4, T>& p_lhs, const Vec<4, T>& p_rhs) {
	p_lhs.x -= p_rhs.x;
	p_lhs.y -= p_rhs.y;
	p_lhs.z -= p_rhs.z;
	p_lhs.w -= p_rhs.w;
	return p_lhs;
}

template <typename T> constexpr Vec<4, T>& operator*=(Vec<4, T>& p_lhs, const T& p_rhs) {
	p_lhs.x *= p_rhs;
	p_lhs.y *= p_rhs;
	p_lhs.z *= p_rhs;
	p_lhs.w *= p_rhs;
	return p_lhs;
}

template <typename T> constexpr Vec<4, T>& operator/=(Vec<4, T>& p_lhs, const T& p_rhs) {
	if (p_rhs == 0.0f) {
		p_lhs = Vec<4, T>(std::numeric_limits<T>::max());
		return p_lhs;
	}

	p_lhs.x /= p_rhs;
	p_lhs.y /= p_rhs;
	p_lhs.z /= p_rhs;
	p_lhs.w /= p_rhs;
	return p_lhs;
}

template <typename T> constexpr Vec<4, T> operator+(Vec<4, T> p_lhs, const Vec<4, T>& p_rhs) {
	return p_lhs += p_rhs;
}

template <typename T> constexpr Vec<4, T> operator-(Vec<4, T> p_lhs, const Vec<4, T>& p_rhs) {
	return p_lhs -= p_rhs;
}

template <typename T> constexpr Vec<4, T> operator*(Vec<4, T> p_lhs, const T& p_rhs) {
	return p_lhs *= p_rhs;
}

template <typename T> constexpr Vec<4, T> operator/(Vec<4, T> p_lhs, const T& p_rhs) {
	return p_lhs /= p_rhs;
}

using Vec4f = Vec<4, float>;
using Vec4d = Vec<4, double>;
using Vec4i = Vec<4, int>;
using Vec4u = Vec<4, uint32_t>;

} // namespace gl
