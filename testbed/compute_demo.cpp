#include "glgpu/backend.h"
#include "glgpu/log.h"

using namespace gl;

std::vector<uint32_t> load_spirv_file(const std::string& filename) {
	size_t file_size = std::filesystem::file_size(filename);

	std::ifstream file(filename, std::ios::in | std::ios::binary);
	if (!file.is_open()) {
		GL_LOG_ERROR("Unable to open SPIRV file at path: '{}'.", filename);
		return {};
	}

	std::vector<uint32_t> buffer(file_size);
	file.read(reinterpret_cast<char*>(buffer.data()), file_size);
	return buffer;
}

int main(void) {
	RenderBackendCreateInfo info = {
		.required_features = gl::RENDER_BACKEND_FEATURE_DISTINCT_COMPUTE_QUEUE_BIT,
	};

	auto backend = RenderBackend::create(info);
	GL_LOG_INFO("Headless backend initialized.");

	// We will process 1024 floats
	const uint32_t element_count = 1024;
	const uint64_t buffer_size = element_count * sizeof(float);

	// Create a buffer that is writable by the shader (STORAGE) and readable by CPU (CPU allocation)
	// Creating this buffer in CPU with TRANSFER_SRC_BIT indicates a staging buffer but for our
	// purposes we are going to be using the same buffer in the GPU. Normally you would create
	// another buffer with BUFFER_USAGE_STORAGE_BUFFER_BIT | BUFFER_USAGE_TRANSFER_DST_BIT as
	// MemoryAllocationType::GPUs
	Buffer storage_buffer = backend->buffer_create(
			buffer_size, BUFFER_USAGE_STORAGE_BUFFER_BIT, MemoryAllocationType::CPU);

	float* raw_data = (float*)backend->buffer_map(storage_buffer);
	if (raw_data) {
		for (uint32_t i = 0; i < element_count; i++) {
			raw_data[i] = (float)i; // Fill with 0, 1, 2, ... 1023
		}
		backend->buffer_unmap(storage_buffer);
	} else {
		GL_LOG_FATAL("Failed to map buffer!");
		return 1;
	}

	std::vector<uint32_t> spirv_code = load_spirv_file("testbed/compute.spv");
	if (spirv_code.empty()) {
		GL_LOG_FATAL("Could not load compute.spv. Did you compile the slang file?");
		return 1;
	}

	// Wrap spirv data (assuming backend expects a specific struct wrapper)
	// The API signature is: shader_create_from_bytecode(const std::vector<SpirvData>&)
	// We need to construct SpirvData. Assuming SpirvData holds stage and code.
	SpirvEntry spirv_entry;
	spirv_entry.byte_code = spirv_code;
	spirv_entry.stage = SHADER_STAGE_COMPUTE;

	Shader compute_shader = backend->shader_create_from_bytecode({ spirv_entry });

	Pipeline compute_pipeline = backend->compute_pipeline_create(compute_shader);

	// We need to tell the shader that binding 0 is our 'storage_buffer'

	// Construct the uniform definition
	// Note: Assuming 'ShaderUniform' struct structure based on common usage
	ShaderUniform buffer_uniform;
	buffer_uniform.binding = 0;
	buffer_uniform.type = UNIFORM_TYPE_STORAGE_BUFFER;
	buffer_uniform.data.push_back(storage_buffer);

	// Create the set (set index 0)
	UniformSet uniform_set = backend->uniform_set_create({ buffer_uniform }, compute_shader, 0);

	// Commands
	CommandQueue compute_queue = backend->queue_get(QueueType::GRAPHICS);
	CommandPool cmd_pool = backend->command_pool_create(compute_queue);
	CommandBuffer cmd = backend->command_pool_allocate(cmd_pool);

	Fence fence = backend->fence_create(false);

	backend->command_begin(cmd);

	// Bind Pipeline
	backend->command_bind_compute_pipeline(cmd, compute_pipeline);

	// Bind Data
	backend->command_bind_uniform_sets(
			cmd, compute_shader, 0, { uniform_set }, PipelineType::COMPUTE);

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

	raw_data = (float*)backend->buffer_map(storage_buffer);
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
