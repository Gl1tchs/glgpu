#include <glgpu/glgpu.h>

std::vector<uint32_t> load_spirv_file(const std::string& filename) {
	std::ifstream file(filename, std::ios::in | std::ios::binary | std::ios::ate);

	if (!file.is_open()) {
		GL_LOG_ERROR("Unable to open SPIRV file at path: '{}'.", filename);
		return {};
	}

	size_t file_size = static_cast<size_t>(file.tellg());

	// SPIR-V blob size must be a multiple of 4
	if (file_size % sizeof(uint32_t) != 0) {
		GL_LOG_ERROR("SPIRV file size is not a multiple of 4 (corrupted?): '{}'.", filename);
		return {};
	}

	// Allocate the buffer with the exact number of 32-bit words
	std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));

	// Reset cursor to beginning
	file.seekg(0);

	// Read directly into the buffer
	file.read(reinterpret_cast<char*>(buffer.data()), file_size);

	return buffer;
}

int main(void) {
	gl::DeviceCreateInfo info = {
		.required_features = gl::RENDER_BACKEND_FEATURE_DISTINCT_COMPUTE_QUEUE_BIT,
	};

	auto backend = gl::Device::create(info).value();
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
			backend->buffer_create(buffer_size, gl::BUFFER_USAGE_STORAGE_BUFFER_BIT,
						   gl::MemoryAllocationType::CPU)
					.value();

	float* raw_data = (float*)backend->buffer_map(storage_buffer).value();
	if (raw_data) {
		for (uint32_t i = 0; i < element_count; i++) {
			raw_data[i] = (float)i; // Fill with 0, 1, 2, ... 1023
		}
		backend->buffer_unmap(storage_buffer);
	} else {
		GL_LOG_FATAL("Failed to map buffer!");
		return 1;
	}

	std::vector<uint32_t> spirv_code = load_spirv_file("examples/assets/compute.spv");
	if (spirv_code.empty()) {
		GL_LOG_FATAL("Could not load compute.spv. Did you compile the slang file?");
		return 1;
	}

	// Wrap spirv data (assuming backend expects a specific struct wrapper)
	// The API signature is: shader_create_from_bytecode(const std::vector<SpirvData>&)
	// We need to construct SpirvData. Assuming SpirvData holds stage and code.
	gl::SpirvEntry spirv_entry;
	spirv_entry.byte_code = spirv_code;
	spirv_entry.stage = gl::SHADER_STAGE_COMPUTE_BIT;

	gl::Shader compute_shader = backend->shader_create_from_bytecode({ spirv_entry }).value();

	gl::Pipeline compute_pipeline = backend->compute_pipeline_create(compute_shader).value();

	// We need to tell the shader that binding 0 is our 'storage_buffer'

	// Construct the uniform definition
	// Note: Assuming 'ShaderUniform' struct structure based on common usage
	gl::ShaderUniform buffer_uniform;
	buffer_uniform.binding = 0;
	buffer_uniform.type = gl::ShaderUniformType::STORAGE_BUFFER;
	buffer_uniform.data.push_back(storage_buffer);

	// Create the set (set index 0)
	gl::UniformSet uniform_set =
			backend->uniform_set_create(buffer_uniform, compute_shader, 0).value();

	// Commands
	gl::CommandQueue compute_queue = backend->queue_get(gl::QueueType::GRAPHICS).value();
	gl::CommandPool cmd_pool = backend->command_pool_create(compute_queue).value();
	gl::CommandBuffer cmd = backend->command_pool_allocate(cmd_pool).value();

	gl::Fence fence = backend->fence_create(false);

	backend->command_begin(cmd);

	// Bind Pipeline
	backend->command_bind_compute_pipeline(cmd, compute_pipeline);

	// Bind Data
	backend->command_bind_uniform_sets(
			cmd, compute_shader, 0, uniform_set, gl::PipelineType::COMPUTE);

	// Dispatch
	// Local size is 64 (defined in slang), so we need 1024 / 64 = 16 groups.
	backend->command_dispatch(cmd, element_count / 64, 1, 1);

	backend->command_end(cmd);

	// Execution
	GL_LOG_INFO("Dispatching compute shader...");

	backend->queue_submit(compute_queue, cmd, fence);

	// Wait for GPU to finish
	backend->fence_wait(fence);

	// Readback and Verify ---
	GL_LOG_INFO("Compute finished. Verifying results...");

	backend->buffer_invalidate(storage_buffer);

	raw_data = (float*)backend->buffer_map(storage_buffer).value();
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

	backend->buffer_unmap(storage_buffer);

	// Cleanup
	// Order matters (usually reverse of creation)
	backend->fence_free(fence);
	backend->command_pool_free(cmd_pool);
	backend->uniform_set_free(uniform_set);
	backend->pipeline_free(compute_pipeline);
	backend->shader_free(compute_shader);
	backend->buffer_free(storage_buffer);

	return success ? 0 : 1;
}
