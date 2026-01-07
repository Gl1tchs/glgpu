#pragma once

#include "glgpu/assert.h"

#include <array>
#include <vector>

namespace gl {

/**
 * A very light weight span-like vector abstraction.
 * It is a non-owning view: ensure the data outlives this object!
 */
template <typename T> class VectorView {
public:
	constexpr VectorView() = default;
	constexpr VectorView(const VectorView&) = default;
	VectorView& operator=(const VectorView&) = default;

	constexpr VectorView(const T& p_val) : _ptr(&p_val), _count(1) {}
	VectorView(const T&&) = delete;

	constexpr VectorView(const T* p_ptr, size_t p_count) : _ptr(p_ptr), _count(p_count) {}

	VectorView(const std::vector<T>& p_vec) : _ptr(p_vec.data()), _count(p_vec.size()) {}

	template <size_t N>
	constexpr VectorView(const std::array<T, N>& p_arr) : _ptr(p_arr.data()), _count(N) {}

	[[nodiscard]] constexpr const T* data() const { return _ptr; }

	const T& operator[](size_t p_index) const {
		GL_ASSERT(p_index < _count);
		return _ptr[p_index];
	}

	[[nodiscard]] constexpr size_t size() const { return _count; }
	[[nodiscard]] constexpr bool empty() const { return _count == 0; }

	constexpr const T* begin() const { return _ptr; }
	constexpr const T* end() const { return _ptr + _count; }

private:
	const T* _ptr = nullptr;
	size_t _count = 0;
};

} //namespace gl
