#pragma once

#include "gpukit/types.h"

namespace gpukit {

// Owns the compiled shader and compute pipeline for a single compute stage.
// Construction loads and compiles; the pipeline is ready to dispatch immediately.
//
// local_size must match the layout(local_size_x = N) declared in the GLSL source (default 64).
class Kernel {
public:
	static constexpr uint32_t DEFAULT_LOCAL_SIZE = 64;

	explicit Kernel(const char* compute_filepath, uint32_t local_size = DEFAULT_LOCAL_SIZE);
	~Kernel();

	Kernel(const Kernel&) = delete;
	Kernel& operator=(const Kernel&) = delete;

	Kernel(Kernel&&) noexcept;
	Kernel& operator=(Kernel&&) noexcept;

	// Compile a kernel from a raw GLSL source string.
	static Kernel from_source(const char* glsl_source, uint32_t local_size = DEFAULT_LOCAL_SIZE);

	Shader shader() const { return _shader; }
	Pipeline pipeline() const { return _pipeline; }
	uint32_t local_size() const { return _local_size; }

private:
	Kernel() = default;

	uint32_t _local_size = DEFAULT_LOCAL_SIZE;
	Shader _shader = GL_NULL_HANDLE;
	Pipeline _pipeline = GL_NULL_HANDLE;
};

} // namespace gpukit
