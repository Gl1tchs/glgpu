#pragma once

#include "gpukit/device.h"
#include "gpukit/types.h"
#include "gpukit/vector_view.h"

#include <array>

namespace gpukit {

enum class TensorMemory {
	HOST, // CPU-visible; zero-copy on iGPU/APU or ReBAR-enabled dGPU
	DEVICE, // GPU-local; staging required for upload/download
};

namespace compute_detail {
template <bool Cond, typename A, typename B> struct select_type {
	using type = A;
};
template <typename A, typename B> struct select_type<false, A, B> {
	using type = B;
};

Res<> tensor_upload_device(Buffer dst, const void* src, uint64_t bytes);
Res<> tensor_download_impl(Buffer src, void* dst, uint64_t bytes, TensorMemory mem);
} // namespace compute_detail

// Tensor<T, N> — a GPU storage buffer of N-component T elements.
//
//   N = 1 (default): element_type is T       — maps to GLSL scalar/struct arrays
//   N > 1:           element_type is std::array<T, N> — maps to GLSL vecN/ivecN arrays
//
// Examples:
//   Tensor<float>    ↔ GLSL: float data[]         (std430 binding)
//   Tensor<float, 4> ↔ GLSL: vec4  data[]
//   Tensor<int, 3>   ↔ GLSL: ivec3 data[]
template <typename T, uint32_t N = 1> class Tensor {
	static_assert(N >= 1, "Tensor component count N must be at least 1");

public:
	using element_type = typename compute_detail::select_type<N == 1, T, std::array<T, N>>::type;

	static constexpr BufferUsageFlags USAGE = BUFFER_USAGE_STORAGE_BUFFER_BIT |
			BUFFER_USAGE_TRANSFER_SRC_BIT | BUFFER_USAGE_TRANSFER_DST_BIT;

	explicit Tensor(uint64_t count, TensorMemory mem = TensorMemory::DEVICE) :
			_count(count), _mem(mem) {
		const MemoryAllocationType alloc = (mem == TensorMemory::DEVICE)
				? MemoryAllocationType::GPU
				: MemoryAllocationType::CPU;
		_buffer = buffer_create(byte_size(), USAGE, alloc).value();
	}

	~Tensor() {
		if (_buffer != GL_NULL_HANDLE)
			buffer_free(_buffer);
	}

	Tensor(Tensor&& o) noexcept : _buffer(o._buffer), _count(o._count), _mem(o._mem) {
		o._buffer = GL_NULL_HANDLE;
	}

	Tensor& operator=(Tensor&& o) noexcept {
		if (this != &o) {
			if (_buffer != GL_NULL_HANDLE)
				buffer_free(_buffer);
			_buffer = o._buffer;
			_count = o._count;
			_mem = o._mem;
			o._buffer = GL_NULL_HANDLE;
		}
		return *this;
	}

	Tensor(const Tensor&) = delete;
	Tensor& operator=(const Tensor&) = delete;

	Res<> upload(VectorView<element_type> data) {
		const uint64_t bytes = data.size() * sizeof(element_type);
		if (_mem == TensorMemory::HOST)
			return buffer_upload(_buffer, data.data(), bytes);
		return compute_detail::tensor_upload_device(_buffer, data.data(), bytes);
	}

	Res<> download(element_type* out, uint64_t count) const {
		return compute_detail::tensor_download_impl(
				_buffer, out, count * sizeof(element_type), _mem);
	}

	// the number of logical elements (each N scalars wide).
	uint64_t count() const { return _count; }
	uint64_t byte_size() const { return _count * sizeof(element_type); }
	Buffer handle() const { return _buffer; }
	TensorMemory memory_type() const { return _mem; }

private:
	Buffer _buffer = GL_NULL_HANDLE;
	uint64_t _count = 0;
	TensorMemory _mem;
};

} // namespace gpukit
