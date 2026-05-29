#pragma once

#include "gpukit/compute/kernel.h"
#include "gpukit/compute/tensor.h"
#include "gpukit/types.h"

#include <vector>

namespace gpukit {

// Ordered GPU execution context. Accumulates dispatches, submits on sync().
// Between dispatches, pipeline barriers are automatically inserted for all
// buffers touched in prior dispatches (conservative but correct for V1).
class Stream {
public:
	static constexpr uint32_t DEFAULT_LOCAL_SIZE = 64;

	Stream();
	~Stream();

	Stream(const Stream&) = delete;
	Stream& operator=(const Stream&) = delete;

	// Dispatch kernel over n elements using Tensor<T> arguments bound positionally
	// to layout(binding=0), layout(binding=1), ... in the shader.
	// local_size must match the local_size_x declared in the shader (default 64).
	template <typename... Ts>
	Res<> dispatch(const Kernel& kernel, uint32_t n, Tensor<Ts>&... tensors) {
		std::vector<Buffer> bufs = { tensors.handle()... };
		return dispatch_impl(kernel, n, DEFAULT_LOCAL_SIZE, bufs);
	}

	template <typename... Ts>
	Res<> dispatch(const Kernel& kernel, uint32_t n, uint32_t local_size, Tensor<Ts>&... tensors) {
		std::vector<Buffer> bufs = { tensors.handle()... };
		return dispatch_impl(kernel, n, local_size, bufs);
	}

	// Submit accumulated commands to GPU and wait for completion.
	// A no-op if no dispatches have been recorded since the last sync.
	Res<> sync();

private:
	void ensure_recording();
	void free_pending_sets();
	Res<> dispatch_impl(const Kernel& kernel, uint32_t n, uint32_t local_size,
			const std::vector<Buffer>& buffers);

	CommandQueue _queue = GL_NULL_HANDLE;
	CommandPool _pool = GL_NULL_HANDLE;
	CommandBuffer _cmd = GL_NULL_HANDLE;
	Fence _fence = GL_NULL_HANDLE;
	bool _recording = false;
	bool _needs_barrier = false;
	std::vector<UniformSet> _pending_sets;
	std::vector<Buffer> _tracked_buffers;
};

} // namespace gpukit
