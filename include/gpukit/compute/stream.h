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
	Stream();
	~Stream();

	Stream(const Stream&) = delete;
	Stream& operator=(const Stream&) = delete;

	// Dispatch kernel over all elements in the tensors. The element count is taken from
	// the first tensor; local_size is taken from kernel.local_size(). Tensors are bound
	// positionally to layout(binding=0), layout(binding=1), ... in the shader.
	template <typename First, typename... Rest>
	Res<> dispatch(const Kernel& kernel, First& first, Rest&... rest) {
		const uint32_t n = static_cast<uint32_t>(first.count());
		std::vector<Buffer> bufs = { first.handle(), rest.handle()... };
		return dispatch_impl(kernel, n, kernel.local_size(), bufs);
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
