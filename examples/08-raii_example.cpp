#include <gpukit/raii/raii.h>

#include <cmath>
#include <cstdint>
#include <vector>

namespace raii = gpukit::raii;

// Returns an owning raii::Buffer already uploaded with data
static std::optional<raii::Buffer> make_storage_buffer(const std::vector<float>& data) {
	const uint64_t size = data.size() * sizeof(float);

	// Create buffer using raw gpukit API and wrap in RAII
	auto buf_res = gpukit::buffer_create(size,
			gpukit::BUFFER_USAGE_STORAGE_BUFFER_BIT | gpukit::BUFFER_USAGE_TRANSFER_DST_BIT,
			gpukit::MemoryAllocationType::CPU);

	if (!buf_res)
		return std::nullopt;

	raii::Buffer buf{ buf_res.own() };

	float* ptr = reinterpret_cast<float*>(gpukit::buffer_map(buf).value());
	for (size_t i = 0; i < data.size(); i++)
		ptr[i] = data[i];
	gpukit::buffer_unmap(buf);

	return buf; // moves ownership to caller — no copy, no double-free
}

int main() {
	raii::Device device = gpukit::init({}).value();

	constexpr uint32_t N = 1024;
	std::vector<float> input(N);
	for (uint32_t i = 0; i < N; i++)
		input[i] = static_cast<float>(i);

	raii::Buffer storage = make_storage_buffer(input).value();

	// Create shader using standalone create function
	raii::Shader shader = gpukit::shader_create("examples/assets/compute.comp").value();

	// Create compute pipeline using standalone create function
	raii::Pipeline pipeline = gpukit::compute_pipeline_create(shader).value();

	gpukit::ShaderUniform u;
	u.binding = 0;
	u.type = gpukit::ShaderUniformType::STORAGE_BUFFER;
	u.data.push_back((gpukit::Buffer)storage); // implicit conversion storage -> Buffer

	// Create uniform set using standalone function
	raii::UniformSet uniform_set =
			gpukit::uniform_set_create(std::vector<gpukit::ShaderUniform>{ u }, shader, 0).value();

	gpukit::CommandQueue queue = gpukit::queue_get(gpukit::QueueType::GRAPHICS).value();

	raii::CommandPool cmd_pool = gpukit::command_pool_create(queue).value();

	gpukit::CommandBuffer cmd = gpukit::command_pool_allocate(cmd_pool).value();

	raii::Fence fence = gpukit::fence_create(false);

	// Dispatch

	gpukit::command_begin(cmd);
	gpukit::command_bind_compute_pipeline(cmd, pipeline);
	gpukit::command_bind_uniform_sets(cmd, shader, 0,
			std::vector<gpukit::UniformSet>{ uniform_set }, gpukit::PipelineType::COMPUTE);
	gpukit::command_dispatch(cmd, N / 64, 1, 1);
	gpukit::command_end(cmd);

	gpukit::queue_submit(queue, cmd, fence); // implicit conversions throughout
	gpukit::fence_wait(fence);

	gpukit::buffer_invalidate(storage);
	const float* result = reinterpret_cast<const float*>(gpukit::buffer_map(storage).value());

	bool ok = true;
	for (uint32_t i = 0; i < N; i++) {
		if (std::fabs(result[i] - static_cast<float>(i) * static_cast<float>(i)) > 0.001f) {
			GPUKIT_LOG_ERROR("Mismatch at {}: expected {}, got {}", i,
					static_cast<float>(i) * static_cast<float>(i), result[i]);
			ok = false;
			break;
		}
	}

	gpukit::buffer_unmap(storage);

	if (ok) {
		GPUKIT_LOG_INFO("SUCCESS — all {} values squared correctly.", N);
	}

	// Explicit free with error inspection
	if (auto res = pipeline.free(); res.is_error()) {
		GPUKIT_LOG_WARNING("pipeline_free error: {}", static_cast<uint32_t>(res.error()));
	}

	return ok ? 0 : 1;
}
