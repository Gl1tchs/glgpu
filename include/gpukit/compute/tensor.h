#pragma once

#include "gpukit/device.h"
#include "gpukit/types.h"
#include "gpukit/vector_view.h"

namespace gpukit {

enum class TensorMemory {
	HOST, // CPU-visible; zero-copy on iGPU/APU or ReBAR-enabled dGPU
	DEVICE, // GPU-local; staging required for upload/download
};

namespace compute_detail {
Res<> tensor_upload_device(Buffer dst, const void* src, uint64_t bytes);
Res<> tensor_download_impl(Buffer src, void* dst, uint64_t bytes, TensorMemory mem);
} // namespace compute_detail

template <typename T> class Tensor {
public:
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
		if (_buffer != GL_NULL_HANDLE) {
			buffer_free(_buffer);
		}
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

	Res<> upload(VectorView<T> data) {
		const uint64_t bytes = data.size() * sizeof(T);
		if (_mem == TensorMemory::HOST) {
			return buffer_upload(_buffer, data.data(), bytes);
		}
		return compute_detail::tensor_upload_device(_buffer, data.data(), bytes);
	}

	Res<> download(T* out, uint64_t count) const {
		return compute_detail::tensor_download_impl(_buffer, out, count * sizeof(T), _mem);
	}

	uint64_t count() const { return _count; }
	uint64_t byte_size() const { return _count * sizeof(T); }
	Buffer handle() const { return _buffer; }
	TensorMemory memory_type() const { return _mem; }

private:
	Buffer _buffer = GL_NULL_HANDLE;
	uint64_t _count = 0;
	TensorMemory _mem;
};

} // namespace gpukit
