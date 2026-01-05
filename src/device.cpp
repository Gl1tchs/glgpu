#include "glgpu/device.h"

#include "platform/vulkan/vk_device.h"

namespace gl {

Res<std::shared_ptr<Device>> Device::create(const DeviceCreateInfo& info) {
	std::shared_ptr<Device> backend = std::make_shared<VulkanDevice>();

	if (auto r = backend->init(info); !r) {
		return make_err<std::shared_ptr<Device>>(r.error());
	}

	return backend;
}

} //namespace gl
