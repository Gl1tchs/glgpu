#pragma once

#include "glgpu/vec.h"

namespace gl {

inline constexpr float as_radians(float p_degrees) {
	constexpr float deg_to_rad = M_PI / 180.0f;
	return p_degrees * deg_to_rad;
}

template <typename T> inline constexpr Vec2<T> pow(const Vec2<T>& p_v, T p_power) {
	return Vec2<T>(std::pow(p_v.x, p_power), std::pow(p_v.y, p_power));
}

template <typename T> inline constexpr Vec3<T> pow(const Vec3<T>& p_v, T p_power) {
	return Vec3<T>(std::pow(p_v.x, p_power), std::pow(p_v.y, p_power), std::pow(p_v.z, p_power));
}

template <typename T> inline constexpr Vec3<T> pow(const Vec4<T>& p_v, T p_power) {
	return Vec3<T>(std::pow(p_v.x, p_power), std::pow(p_v.y, p_power), std::pow(p_v.z, p_power),
			std::pow(p_v.w, p_power));
}

} //namespace gl
