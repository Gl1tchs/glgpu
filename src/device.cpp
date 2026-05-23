#include "gpukit/device.h"

#include "gpukit/version.h"
#include "platform/vulkan/vk_device.h"

namespace gpukit {

Res<std::unique_ptr<Device>> Device::create(const DeviceCreateInfo& info) {
	GPUKIT_LOG_INFO("[GPUKit] Version {}", GPUKIT_VERSION_STR);

	std::unique_ptr<Device> backend = std::make_unique<VulkanDevice>();
	if (auto r = backend->init(info); !r) {
		return make_err<std::unique_ptr<Device>>(r.error());
	}

	return backend;
}

} //namespace gpukitkit
