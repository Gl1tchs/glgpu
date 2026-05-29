#include "gpukit/compute/tensor.h"

#include <cstring>

namespace gpukit::compute_detail {

Res<> tensor_upload_device(Buffer dst, const void* src, uint64_t bytes) {
	auto staging_res =
			buffer_create(bytes, BUFFER_USAGE_TRANSFER_SRC_BIT, MemoryAllocationType::CPU);
	if (!staging_res)
		return Res<>(staging_res.error());

	Buffer staging = staging_res.value();

	if (auto r = buffer_upload(staging, src, bytes); !r) {
		buffer_free(staging);
		return r;
	}

	Res<> result = command_immediate_submit(
			[&](CommandBuffer cmd) {
				BufferCopyRegion region = { 0, 0, bytes };
				command_copy_buffer(cmd, staging, dst, region);
			},
			QueueType::TRANSFER);

	buffer_free(staging);
	return result;
}

Res<> tensor_download_impl(Buffer src, void* dst, uint64_t bytes, TensorMemory mem) {
	if (mem == TensorMemory::HOST) {
		if (auto r = buffer_invalidate(src); !r)
			return r;
		auto map_res = buffer_map(src);
		if (!map_res)
			return Res<>(map_res.error());
		std::memcpy(dst, map_res.value(), bytes);
		buffer_unmap(src);
		return {};
	}

	// DEVICE: copy to a CPU-visible staging buffer then read back
	auto staging_res =
			buffer_create(bytes, BUFFER_USAGE_TRANSFER_DST_BIT, MemoryAllocationType::CPU);
	if (!staging_res)
		return Res<>(staging_res.error());
	Buffer staging = staging_res.value();

	Res<> copy_result = command_immediate_submit(
			[&](CommandBuffer cmd) {
				BufferCopyRegion region = { 0, 0, bytes };
				command_copy_buffer(cmd, src, staging, region);
			},
			QueueType::TRANSFER);

	if (!copy_result) {
		buffer_free(staging);
		return copy_result;
	}

	if (auto r = buffer_invalidate(staging); !r) {
		buffer_free(staging);
		return r;
	}

	auto map_res = buffer_map(staging);
	if (!map_res) {
		buffer_free(staging);
		return Res<>(map_res.error());
	}

	std::memcpy(dst, map_res.value(), bytes);
	buffer_unmap(staging);
	buffer_free(staging);
	return {};
}

} // namespace gpukit::compute_detail
