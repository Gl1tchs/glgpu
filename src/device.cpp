#include "glgpu/device.h"

#include "glgpu/version.h"
#include "platform/vulkan/vk_device.h"

namespace gl {

Res<std::unique_ptr<Device>> Device::create(const DeviceCreateInfo& info) {
	GL_LOG_INFO("[GLGPU] Version {}", GLGPU_VERSION_STR);

	std::unique_ptr<Device> backend = std::make_unique<VulkanDevice>();
	if (auto r = backend->init(info); !r) {
		return make_err<std::unique_ptr<Device>>(r.error());
	}

	return backend;
}

} //namespace gl
