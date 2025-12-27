#include "glgpu/backend.h"

#include "platform/vulkan/vk_backend.h"

namespace gl {

Res<std::shared_ptr<RenderBackend>> RenderBackend::create(const RenderBackendCreateInfo& info) {
	std::shared_ptr<RenderBackend> backend = std::make_shared<VulkanRenderBackend>();

	Res<> res = backend->init(info);
	if (res.is_error()) {
		return make_err<std::shared_ptr<RenderBackend>>(res.error());
	}

	return backend;
}

} //namespace gl
