#include <glgpu/glgpu.h>

#include <cstdint>
#include <vector>

int main(void) {
	gl::DeviceCreateInfo info = {
		.required_features = gl::DEVICE_FEATURE_DISTINCT_COMPUTE_QUEUE_BIT,
	};

	auto device = gl::Device::create(info).own();
	GL_LOG_INFO("Headless backend initialized.");

	// We will process 1024 floats
	const uint32_t element_count = 1024;
	const uint64_t buffer_size = element_count * sizeof(float);

	// Create a buffer that is writable by the shader (STORAGE) and readable by CPU (CPU allocation)
	// Creating this buffer in CPU with TRANSFER_SRC_BIT indicates a staging buffer but for our
	// purposes we are going to be using the same buffer in the GPU. Normally you would create
	// another buffer with BUFFER_USAGE_STORAGE_BUFFER_BIT | BUFFER_USAGE_TRANSFER_DST_BIT as
	// MemoryAllocationType::GPUs
	gl::Buffer storage_buffer =
			device->buffer_create(buffer_size, gl::BUFFER_USAGE_STORAGE_BUFFER_BIT,
						  gl::MemoryAllocationType::CPU)
					.value();

	float* raw_data = (float*)device->buffer_map(storage_buffer).value();
	if (raw_data) {
		for (uint32_t i = 0; i < element_count; i++) {
			raw_data[i] = (float)i; // Fill with 0, 1, 2, ... 1023
		}
		device->buffer_unmap(storage_buffer);
	} else {
		GL_LOG_FATAL("Failed to map buffer!");
		return 1;
	}

	gl::Shader compute_shader = device->shader_create("examples/assets/compute.glsl").value();

	gl::Pipeline compute_pipeline = device->compute_pipeline_create(compute_shader).value();

	// We need to tell the shader that binding 0 is our 'storage_buffer'

	// Construct the uniform definition
	// Note: Assuming 'ShaderUniform' struct structure based on common usage
	gl::ShaderUniform buffer_uniform;
	buffer_uniform.binding = 0;
	buffer_uniform.type = gl::ShaderUniformType::STORAGE_BUFFER;
	buffer_uniform.data.push_back(storage_buffer);

	// Create the set (set index 0)
	gl::UniformSet uniform_set =
			device->uniform_set_create(buffer_uniform, compute_shader, 0).value();

	// Commands
	gl::CommandQueue compute_queue = device->queue_get(gl::QueueType::GRAPHICS).value();
	gl::CommandPool cmd_pool = device->command_pool_create(compute_queue).value();
	gl::CommandBuffer cmd = device->command_pool_allocate(cmd_pool).value();

	gl::Fence fence = device->fence_create(false);

	device->command_begin(cmd);

	// Bind Pipeline
	device->command_bind_compute_pipeline(cmd, compute_pipeline);

	// Bind Data
	device->command_bind_uniform_sets(
			cmd, compute_shader, 0, uniform_set, gl::PipelineType::COMPUTE);

	// Dispatch
	// Local size is 64 (defined in slang), so we need 1024 / 64 = 16 groups.
	device->command_dispatch(cmd, element_count / 64, 1, 1);

	device->command_end(cmd);

	// Execution
	GL_LOG_INFO("Dispatching compute shader...");

	device->queue_submit(compute_queue, cmd, fence);

	// Wait for GPU to finish
	device->fence_wait(fence);

	// Readback and Verify ---
	GL_LOG_INFO("Compute finished. Verifying results...");

	device->buffer_invalidate(storage_buffer);

	raw_data = (float*)device->buffer_map(storage_buffer).value();
	bool success = true;

	for (uint32_t i = 0; i < element_count; i++) {
		float input = (float)i;
		float expected = input * input; // The shader squares the number
		float actual = raw_data[i];

		if (std::abs(actual - expected) > 0.001f) {
			GL_LOG_ERROR("Mismatch at index {}: Expected {}, Got {}", i, expected, actual);
			success = false;
			break;
		}
	}

	if (success) {
		GL_LOG_INFO("SUCCESS! All {} values were squared correctly on the GPU.", element_count);
	} else {
		GL_LOG_ERROR("FAILURE! Compute results were incorrect.");
	}

	device->buffer_unmap(storage_buffer);

	// Cleanup
	// Order matters (usually reverse of creation)
	device->fence_free(fence);
	device->command_pool_free(cmd_pool);
	device->uniform_set_free(uniform_set);
	device->pipeline_free(compute_pipeline);
	device->shader_free(compute_shader);
	device->buffer_free(storage_buffer);

	return success ? 0 : 1;
}
