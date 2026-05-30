#include "gpukit/compute/kernel.h"

#include "gpukit/device.h"

namespace gpukit {

Kernel::Kernel(const char* compute_filepath, uint32_t local_size) : _local_size(local_size) {
	_shader = shader_create(compute_filepath).value();
	_pipeline = compute_pipeline_create(_shader).value();
}

Kernel::~Kernel() {
	if (_pipeline != GL_NULL_HANDLE)
		pipeline_free(_pipeline);
	if (_shader != GL_NULL_HANDLE)
		shader_free(_shader);
}

Kernel::Kernel(Kernel&& o) noexcept :
		_local_size(o._local_size), _shader(o._shader), _pipeline(o._pipeline) {
	o._shader = GL_NULL_HANDLE;
	o._pipeline = GL_NULL_HANDLE;
}

Kernel& Kernel::operator=(Kernel&& o) noexcept {
	if (this != &o) {
		if (_pipeline != GL_NULL_HANDLE)
			pipeline_free(_pipeline);
		if (_shader != GL_NULL_HANDLE)
			shader_free(_shader);
		_local_size = o._local_size;
		_shader = o._shader;
		_pipeline = o._pipeline;
		o._shader = GL_NULL_HANDLE;
		o._pipeline = GL_NULL_HANDLE;
	}
	return *this;
}

Kernel Kernel::from_source(const char* glsl_source, uint32_t local_size) {
	Kernel k;
	k._local_size = local_size;
	k._shader = shader_create_from_source(glsl_source, SHADER_STAGE_COMPUTE_BIT).value();
	k._pipeline = compute_pipeline_create(k._shader).value();
	return k;
}

} // namespace gpukit
