#pragma once

#include "glgpu/vector.h"

namespace gl {

inline constexpr float as_radians(float p_degrees) {
	constexpr float deg_to_rad = M_PI / 180.0f;
	return p_degrees * deg_to_rad;
}

template <size_t S, typename T> inline constexpr Vec<S, T> pow(Vec<S, T> p_v, T p_power) {
	for (size_t i = 0; i < S; i++) {
		p_v[i] = std::pow(p_v[i], p_power);
	}
	return p_v;
}

template <size_t S, typename T>
inline constexpr Vec<S, T> min(Vec<S, T> p_lhs, const Vec<S, T>& p_rhs) {
	for (size_t i = 0; i < S; i++) {
		p_lhs[i] = std::min(p_lhs[i], p_rhs[i]);
	}
	return p_lhs;
}
template <size_t S, typename T> inline constexpr Vec<S, T> min(Vec<S, T> p_v, T p_scalar) {
	for (size_t i = 0; i < S; i++) {
		p_v[i] = std::min(p_v[i], p_scalar);
	}
	return p_v;
}

template <size_t S, typename T>
inline constexpr Vec<S, T> max(Vec<S, T> p_lhs, const Vec<S, T>& p_rhs) {
	for (size_t i = 0; i < S; i++) {
		p_lhs[i] = std::max(p_lhs[i], p_rhs[i]);
	}
	return p_lhs;
}
template <size_t S, typename T> inline constexpr Vec<S, T> max(Vec<S, T> p_v, T p_scalar) {
	for (size_t i = 0; i < S; i++) {
		p_v[i] = std::max(p_v[i], p_scalar);
	}
	return p_v;
}

template <size_t S, typename T>
inline constexpr Vec<S, T> clamp(const Vec<S, T>& p_v, T p_lower, T p_upper) {
	return min(max(p_v, p_lower), p_upper);
}

} //namespace gl
