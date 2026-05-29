#include "gpukit/compute/kernel.h"

namespace gpukit {

Kernel::Kernel(const char* compute_filepath) {
	_shader = shader_create(compute_filepath).value();
	_pipeline = compute_pipeline_create(_shader).value();
}

Kernel::~Kernel() {
	if (_pipeline != GL_NULL_HANDLE)
		pipeline_free(_pipeline);
	if (_shader != GL_NULL_HANDLE)
		shader_free(_shader);
}

Kernel::Kernel(Kernel&& o) noexcept : _shader(o._shader), _pipeline(o._pipeline) {
	o._shader = GL_NULL_HANDLE;
	o._pipeline = GL_NULL_HANDLE;
}

Kernel& Kernel::operator=(Kernel&& o) noexcept {
	if (this != &o) {
		if (_pipeline != GL_NULL_HANDLE)
			pipeline_free(_pipeline);
		if (_shader != GL_NULL_HANDLE)
			shader_free(_shader);
		_shader = o._shader;
		_pipeline = o._pipeline;
		o._shader = GL_NULL_HANDLE;
		o._pipeline = GL_NULL_HANDLE;
	}
	return *this;
}

} // namespace gpukit
