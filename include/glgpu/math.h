#pragma once

namespace gl {

inline constexpr float as_radians(float p_degrees) {
	constexpr float deg_to_rad = M_PI / 180.0f;
	return p_degrees * deg_to_rad;
}

} //namespace gl
