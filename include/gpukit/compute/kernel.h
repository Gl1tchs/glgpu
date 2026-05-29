#pragma once

#include "gpukit/device.h"
#include "gpukit/types.h"

namespace gpukit {

// Owns the compiled shader and compute pipeline for a single .comp GLSL file.
// Construction loads and compiles; the pipeline is ready to dispatch immediately.
class Kernel {
public:
	explicit Kernel(const char* compute_filepath);
	~Kernel();

	Kernel(const Kernel&) = delete;
	Kernel& operator=(const Kernel&) = delete;

	Kernel(Kernel&&) noexcept;
	Kernel& operator=(Kernel&&) noexcept;

	Shader shader() const { return _shader; }
	Pipeline pipeline() const { return _pipeline; }

private:
	Shader _shader = GL_NULL_HANDLE;
	Pipeline _pipeline = GL_NULL_HANDLE;
};

} // namespace gpukit
