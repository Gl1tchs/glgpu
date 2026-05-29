#include <gpukit/gpukit.h>

#define GPUKIT_EXPOSE_VULKAN
#include <gpukit/vulkan.h>

static const char* device_type_string(VkPhysicalDeviceType type) {
	switch (type) {
		case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
			return "Integrated GPU";
		case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
			return "Discrete GPU";
		case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
			return "Virtual GPU";
		case VK_PHYSICAL_DEVICE_TYPE_CPU:
			return "CPU";
		default:
			return "Other";
	}
}

int main() {
	gpukit::DeviceCreateInfo info = {};
	if (!gpukit::init(info)) {
		GPUKIT_LOG_FATAL("Failed to initialize gpukit.");
		return 1;
	}

	VkPhysicalDevice physical_device = gpukit::vk::vulkan_get_physical_device();

	VkPhysicalDeviceProperties props = {};
	vkGetPhysicalDeviceProperties(physical_device, &props);

	GPUKIT_LOG_INFO("Physical Device Properties");
	GPUKIT_LOG_INFO("  Name:          {}", props.deviceName);
	GPUKIT_LOG_INFO("  Type:          {}", device_type_string(props.deviceType));
	GPUKIT_LOG_INFO("  API Version:   {}.{}.{}", VK_VERSION_MAJOR(props.apiVersion),
			VK_VERSION_MINOR(props.apiVersion), VK_VERSION_PATCH(props.apiVersion));
	GPUKIT_LOG_INFO("  Driver Ver:    {}", props.driverVersion);
	GPUKIT_LOG_INFO("  Vendor ID:     0x{:04X}", props.vendorID);
	GPUKIT_LOG_INFO("  Device ID:     0x{:04X}", props.deviceID);
	GPUKIT_LOG_INFO("  Max 2D Dim:    {}", props.limits.maxImageDimension2D);
	GPUKIT_LOG_INFO("  Max UBO Range: {}", props.limits.maxUniformBufferRange);
	GPUKIT_LOG_INFO("  Push Consts:   {} bytes", props.limits.maxPushConstantsSize);

	// Buffer handle unwrapping.
	gpukit::Buffer buf = gpukit::buffer_create(
			256, gpukit::BUFFER_USAGE_STORAGE_BUFFER_BIT, gpukit::MemoryAllocationType::GPU)
								 .value();

	gpukit::vk::VulkanBufferInfo buf_info = gpukit::vk::vulkan_get_buffer_info(buf).value();
	GPUKIT_LOG_INFO("VkBuffer handle for 256-byte storage buffer: 0x{:016X}",
			reinterpret_cast<uint64_t>(buf_info.buffer));

	gpukit::buffer_free(buf);

	gpukit::shutdown();
	return 0;
}
