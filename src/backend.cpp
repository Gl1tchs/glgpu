#include "glgpu/backend.h"

#include "platform/vulkan/vk_backend.h"

namespace gl {

std::shared_ptr<RenderBackend> RenderBackend::create(const RenderBackendCreateInfo& p_info) {
	return std::make_shared<VulkanRenderBackend>(p_info);
}

} //namespace gl