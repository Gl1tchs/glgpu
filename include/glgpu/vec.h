#pragma once

namespace gl {

template <typename T> struct Vec2 {
	T x, y;
};

typedef Vec2<float> Vec2f;
typedef Vec2<double> Vec2d;
typedef Vec2<int> Vec2i;
typedef Vec2<uint32_t> Vec2u;

template <typename T> struct Vec3 {
	T x, y, z;
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