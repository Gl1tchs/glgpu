#include "gpukit/compute/stream.h"

#include "gpukit/device.h"

#include <algorithm>

namespace gpukit {

Stream::Stream() {
	// Prefer a dedicated compute queue; fall back to graphics if unavailable
	auto queue_res = queue_get(QueueType::COMPUTE);
	_queue = queue_res ? queue_res.value() : queue_get(QueueType::GRAPHICS).value();
	_pool = command_pool_create(_queue).value();
	_cmd = command_pool_allocate(_pool).value();
	_fence = fence_create(false);
}

Stream::~Stream() {
	// Discard any in-flight recording without submitting
	if (_recording) {
		command_end(_cmd);
		_recording = false;
	}
	free_pending_sets();
	fence_free(_fence);
	command_pool_free(_pool);
}

void Stream::ensure_recording() {
	if (_recording)
		return;
	command_pool_reset(_pool);
	command_begin(_cmd);
	_recording = true;
}

void Stream::free_pending_sets() {
	for (UniformSet s : _pending_sets)
		uniform_set_free(s);
	_pending_sets.clear();
}

Res<> Stream::dispatch_impl(
		const Kernel& kernel, uint32_t n, uint32_t local_size, const std::vector<Buffer>& buffers) {
	ensure_recording();

	// Conservative barrier: protect all previously-touched buffers before the next dispatch
	if (_needs_barrier && !_tracked_buffers.empty()) {
		std::vector<BufferBarrier> barriers;
		barriers.reserve(_tracked_buffers.size());
		for (Buffer buf : _tracked_buffers) {
			barriers.push_back({
					buf,
					PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					PIPELINE_STAGE_COMPUTE_SHADER_BIT,
					ACCESS_SHADER_WRITE_BIT,
					ACCESS_SHADER_READ_BIT | ACCESS_SHADER_WRITE_BIT,
			});
		}
		if (auto r = command_pipeline_barrier(_cmd, barriers, {}); !r)
			return r;
	}

	// Build one uniform set binding each buffer positionally to binding=0, 1, 2 ...
	std::vector<ShaderUniform> uniforms;
	uniforms.reserve(buffers.size());
	for (uint32_t i = 0; i < static_cast<uint32_t>(buffers.size()); i++) {
		ShaderUniform u;
		u.binding = i;
		u.type = ShaderUniformType::STORAGE_BUFFER;
		u.data.push_back(buffers[i]);
		uniforms.push_back(std::move(u));
	}

	auto set_res = uniform_set_create(uniforms, kernel.shader(), 0);
	if (!set_res)
		return Res<>(set_res.error());

	_pending_sets.push_back(set_res.value());

	if (auto r = command_bind_compute_pipeline(_cmd, kernel.pipeline()); !r)
		return r;

	if (auto r = command_bind_uniform_sets(
				_cmd, kernel.shader(), 0, _pending_sets.back(), PipelineType::COMPUTE);
			!r)
		return r;

	const uint32_t groups = (n + local_size - 1) / local_size;
	if (auto r = command_dispatch(_cmd, groups, 1, 1); !r)
		return r;

	// Track unique buffers for future barrier insertion
	for (Buffer buf : buffers) {
		if (std::find(_tracked_buffers.begin(), _tracked_buffers.end(), buf) ==
				_tracked_buffers.end()) {
			_tracked_buffers.push_back(buf);
		}
	}

	_needs_barrier = true;

	return {};
}

Res<> Stream::sync() {
	if (!_recording)
		return {};

	if (auto r = command_end(_cmd); !r)
		return r;

	_recording = false;

	if (auto r = fence_reset(_fence); !r)
		return r;
	if (auto r = queue_submit(_queue, _cmd, _fence); !r)
		return r;
	if (auto r = fence_wait(_fence); !r)
		return r;

	free_pending_sets();

	_tracked_buffers.clear();
	_needs_barrier = false;

	return {};
}

} // namespace gpukit
