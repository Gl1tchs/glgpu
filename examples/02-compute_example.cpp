#include <gpukit/gpukit.h>

#include <cstdint>
#include <vector>

int main(void) {
	gpukit::DeviceCreateInfo info = {
		.required_features = gpukit::DEVICE_FEATURE_DISTINCT_COMPUTE_QUEUE_BIT,
	};

	gpukit::init(info);
	GPUKIT_LOG_INFO("Headless backend initialized.");

	const uint32_t element_count = 1024;
	const uint64_t buffer_size = element_count * sizeof(float);

	gpukit::Buffer storage_buffer = gpukit::buffer_create(
			buffer_size, gpukit::BUFFER_USAGE_STORAGE_BUFFER_BIT, gpukit::MemoryAllocationType::CPU)
											.value();

	float* raw_data = (float*)gpukit::buffer_map(storage_buffer).value();
	if (raw_data) {
		for (uint32_t i = 0; i < element_count; i++) {
			raw_data[i] = (float)i;
		}
		gpukit::buffer_unmap(storage_buffer);
	} else {
		GPUKIT_LOG_FATAL("Failed to map buffer!");
		return 1;
	}

	gpukit::Shader compute_shader = gpukit::shader_create("examples/assets/compute.comp").value();

	gpukit::Pipeline compute_pipeline = gpukit::compute_pipeline_create(compute_shader).value();

	gpukit::ShaderUniform buffer_uniform;
	buffer_uniform.binding = 0;
	buffer_uniform.type = gpukit::ShaderUniformType::STORAGE_BUFFER;
	buffer_uniform.data.push_back(storage_buffer);

	gpukit::UniformSet uniform_set =
			gpukit::uniform_set_create(buffer_uniform, compute_shader, 0).value();

	gpukit::CommandQueue compute_queue = gpukit::queue_get(gpukit::QueueType::GRAPHICS).value();
	gpukit::CommandPool cmd_pool = gpukit::command_pool_create(compute_queue).value();
	gpukit::CommandBuffer cmd = gpukit::command_pool_allocate(cmd_pool).value();

	gpukit::Fence fence = gpukit::fence_create(false);

	gpukit::command_begin(cmd);
	gpukit::command_bind_compute_pipeline(cmd, compute_pipeline);
	gpukit::command_bind_uniform_sets(
			cmd, compute_shader, 0, uniform_set, gpukit::PipelineType::COMPUTE);
	gpukit::command_dispatch(cmd, element_count / 64, 1, 1);
	gpukit::command_end(cmd);

	GPUKIT_LOG_INFO("Dispatching compute shader...");
	gpukit::queue_submit(compute_queue, cmd, fence);
	gpukit::fence_wait(fence);

	GPUKIT_LOG_INFO("Compute finished. Verifying results...");

	gpukit::buffer_invalidate(storage_buffer);

	raw_data = (float*)gpukit::buffer_map(storage_buffer).value();
	bool success = true;

	for (uint32_t i = 0; i < element_count; i++) {
		float input = (float)i;
		float expected = input * input;
		float actual = raw_data[i];

		if (std::abs(actual - expected) > 0.001f) {
			GPUKIT_LOG_ERROR("Mismatch at index {}: Expected {}, Got {}", i, expected, actual);
			success = false;
			break;
		}
	}

	if (success) {
		GPUKIT_LOG_INFO("SUCCESS! All {} values were squared correctly on the GPU.", element_count);
	} else {
		GPUKIT_LOG_ERROR("FAILURE! Compute results were incorrect.");
	}

	gpukit::buffer_unmap(storage_buffer);

	gpukit::fence_free(fence);
	gpukit::command_pool_free(cmd_pool);
	gpukit::uniform_set_free(uniform_set);
	gpukit::pipeline_free(compute_pipeline);
	gpukit::shader_free(compute_shader);
	gpukit::buffer_free(storage_buffer);

	gpukit::shutdown();

	return success ? 0 : 1;
}
