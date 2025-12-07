#pragma once

namespace gl {

/**
 * Color representing Red,Green,Blue,Alpha values in 32-bit floats.
 */
struct Color {
	float r = 0.0f;
	float g = 0.0f;
	float b = 0.0f;
	float a = 1.0f;

	/**
	 * Gray-Scale constructor, fills every component
	 * except `alpha` to `p_value`
	 */
	constexpr Color(float value = 0.0f) : r(value), g(value), b(value) {}

	constexpr Color(float red, float green, float blue, float alpha = 1.0f) :
			r(red), g(green), b(blue), a(alpha) {}

	/**
	 * Constructs `Color` object from RRGGBBAA packed unsigned integer
	 */
	constexpr Color(uint32_t value) :
			r(((value >> 24) & 0xFF) / 255.0f),
			g(((value >> 16) & 0xFF) / 255.0f),
			b(((value >> 8) & 0xFF) / 255.0f),
			a((value & 0xFF) / 255.0f) {}

	/**
	 * Get RRGGBBAA packed uint32_t representation of the `Color` object.
	 */
	uint32_t as_uint() const {
		auto to_uint8 = [](float p_value) {
			float result = std::floor(p_value * 256.0f);
			return result > 255.0f ? 255 : static_cast<uint8_t>(result);
		};

		uint8_t red = to_uint8(r);
		uint8_t green = to_uint8(g);
		uint8_t blue = to_uint8(b);
		uint8_t alpha = to_uint8(a);

		return (red << 24) | (green << 16) | (blue << 8) | alpha;
	}

	constexpr bool operator==(const Color& p_other) const {
		return r == p_other.r && g == p_other.g && b == p_other.b && a == p_other.a;
	}
};

constexpr Color COLOR_BLACK(0.0f, 0.0f, 0.0f, 1.0f);
constexpr Color COLOR_WHITE(1.0f, 1.0f, 1.0f, 1.0f);
constexpr Color COLOR_RED(1.0f, 0.0f, 0.0f, 1.0f);
constexpr Color COLOR_GREEN(0.0f, 1.0f, 0.0f, 1.0f);
constexpr Color COLOR_BLUE(0.0f, 0.0f, 1.0f, 1.0f);
constexpr Color COLOR_YELLOW(1.0f, 1.0f, 0.0f, 1.0f);
constexpr Color COLOR_CYAN(0.0f, 1.0f, 1.0f, 1.0f);
constexpr Color COLOR_MAGENTA(1.0f, 0.0f, 1.0f, 1.0f);
constexpr Color COLOR_GRAY(0.5f, 0.5f, 0.5f, 1.0f);
constexpr Color COLOR_ORANGE(1.0f, 0.5f, 0.0f, 1.0f);
constexpr Color COLOR_TRANSPARENT(0.0f, 0.0f, 0.0f, 0.0f);

} //namespace gl