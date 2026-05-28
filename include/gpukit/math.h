#pragma once

#include "gpukit/vector.h"

#include <cmath>

namespace gpukit {

namespace math {

inline constexpr float as_radians(float degrees) {
	constexpr float deg_to_rad = M_PI / 180.0f;
	return degrees * deg_to_rad;
}

template <size_t S, typename T> inline constexpr Vec<S, T> pow(Vec<S, T> v, T power) {
	for (size_t i = 0; i < S; i++) {
		v[i] = std::pow(v[i], power);
	}
	return v;
}

template <size_t S, typename T>
inline constexpr Vec<S, T> min(Vec<S, T> lhs, const Vec<S, T>& rhs) {
	for (size_t i = 0; i < S; i++) {
		lhs[i] = std::min(lhs[i], rhs[i]);
	}
	return lhs;
}
template <size_t S, typename T> inline constexpr Vec<S, T> min(Vec<S, T> v, T scalar) {
	for (size_t i = 0; i < S; i++) {
		v[i] = std::min(v[i], scalar);
	}
	return v;
}

template <size_t S, typename T>
inline constexpr Vec<S, T> max(Vec<S, T> lhs, const Vec<S, T>& rhs) {
	for (size_t i = 0; i < S; i++) {
		lhs[i] = std::max(lhs[i], rhs[i]);
	}
	return lhs;
}
template <size_t S, typename T> inline constexpr Vec<S, T> max(Vec<S, T> v, T scalar) {
	for (size_t i = 0; i < S; i++) {
		v[i] = std::max(v[i], scalar);
	}
	return v;
}

template <size_t S, typename T>
inline constexpr Vec<S, T> clamp(const Vec<S, T>& v, T lower, T upper) {
	return min(max(v, lower), upper);
}

} //namespace math

} //namespace gpukit
