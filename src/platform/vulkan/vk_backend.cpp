#include "platform/vulkan/vk_backend.h"

#include "glgpu/assert.h"
#include "glgpu/backend.h"
#include "platform/vulkan/vk_common.h"

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#if defined(_WIN32)
#include <vulkan/vulkan_win32.h>
#include <windows.h>
#elif defined(__linux__)
#include <X11/Xlib.h>
#include <vulkan/vulkan_xlib.h>
#endif

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

namespace gl {

const std::vector<const char*> VALIDATION_LAYERS = {
	"VK_LAYER_KHRONOS_validation",
};

const std::vector<const char*> DEVICE_EXTENSIONS_REQUIRED = {
	VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
};

static VKAPI_ATTR VkBool32 VKAPI_CALL _vk_debug_callback(
		VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
		VkDebugUtilsMessageTypeFlagsEXT message_type,
		const VkDebugUtilsMessengerCallbackDataEXT* callback_data, void* user_data) {
	if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
		GL_LOG_ERROR("[VULKAN] {}", callback_data->pMessage);
	} else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		GL_LOG_WARNING("[VULKAN] {}", callback_data->pMessage);
	} else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
		GL_LOG_INFO("[VULKAN] {}", callback_data->pMessage);
	} else {
		GL_LOG_TRACE("[VULKAN] {}", callback_data->pMessage);
	}

	return VK_FALSE;
}

static bool s_initialized = false;

VulkanRenderBackend::VulkanRenderBackend(const RenderBackendCreateInfo& p_info) {
	if (s_initialized) {
		GL_ASSERT(false, "Only one backend can exist at a time.");
		return;
	}

#ifdef GL_DEBUG_BUILD
	if (!_check_validation_layer_support()) {
		GL_LOG_WARNING("[VULKAN] Validation layers requested but not available!");
	}
#endif

	VkApplicationInfo app_info = {};
	app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
	app_info.pApplicationName = "Glitch Application";
	app_info.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.pEngineName = "Glitch Engine";
	app_info.engineVersion = VK_MAKE_VERSION(1, 0, 0);
	app_info.apiVersion = VK_API_VERSION_1_3;

	VkInstanceCreateInfo instance_info = {};
	instance_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
	instance_info.pApplicationInfo = &app_info;

	// Get Extensions
	std::vector<const char*> extensions;
	extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#if defined(_WIN32)
	extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(__linux__)
	extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
#endif

#ifdef GL_DEBUG_BUILD
	extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

	instance_info.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
	instance_info.ppEnabledExtensionNames = extensions.data();

	// Validation Layers
	VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {};
#ifdef GL_DEBUG_BUILD
	instance_info.enabledLayerCount = static_cast<uint32_t>(VALIDATION_LAYERS.size());
	instance_info.ppEnabledLayerNames = VALIDATION_LAYERS.data();

	debug_create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
	debug_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
	debug_create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
			VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
	debug_create_info.pfnUserCallback = _vk_debug_callback;

	instance_info.pNext = (VkDebugUtilsMessengerCreateInfoEXT*)&debug_create_info;
#else
	instance_info.enabledLayerCount = 0;
	instance_info.pNext = nullptr;
#endif

	if (vkCreateInstance(&instance_info, nullptr, &instance) != VK_SUCCESS) {
		GL_ASSERT(false, "Failed to create Vulkan Instance");
		return;
	}

#ifdef GL_DEBUG_BUILD
	if (_create_debug_utils_messenger_ext(
				instance, &debug_create_info, nullptr, &debug_messenger) != VK_SUCCESS) {
		GL_LOG_WARNING("[VULKAN] Failed to set up debug messenger!");
	}
#endif

	s_initialized = true;

	const bool swapchain_support_required =
			p_info.required_features & RENDER_BACKEND_FEATURE_SWAPCHAIN_BIT;
	const bool surface_support_required =
			p_info.required_features & RENDER_BACKEND_FEATURE_ENSURE_SURFACE_SUPPORT;
	const bool distinct_compute_queue_required =
			(p_info.required_features & RENDER_BACKEND_FEATURE_DISTINCT_COMPUTE_QUEUE_BIT);

	// Try to create a surface
	if (surface_support_required && !p_info.native_window_handle) {
		GL_ASSERT(false, "Surface support required but no window provided.");
		return;
	}

	if ((swapchain_support_required || surface_support_required) && p_info.native_window_handle) {
		if (!_create_surface_platform_specific(
					p_info.native_connection_handle, p_info.native_window_handle)) {
			GL_ASSERT(false, "Failed to create Window Surface");
			return;
		}
	}

	// Pick GPU
	uint32_t device_count = 0;
	vkEnumeratePhysicalDevices(instance, &device_count, nullptr);
	if (device_count == 0) {
		GL_ASSERT(false, "Failed to find GPUs with Vulkan support!");
		return;
	}
	std::vector<VkPhysicalDevice> devices(device_count);
	vkEnumeratePhysicalDevices(instance, &device_count, devices.data());

	std::vector<const char*> required_extensions(
			DEVICE_EXTENSIONS_REQUIRED.begin(), DEVICE_EXTENSIONS_REQUIRED.end());

	// Add swapchain extension if requested
	if (swapchain_support_required || surface_support_required) {
		required_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	}

	std::multimap<int, std::pair<VkPhysicalDevice, QueueFamilyIndices>> candidates;
	for (const auto& dev : devices) {
		const QueueFamilyIndices indices =
				_find_queue_families(dev, p_info.required_features, surface);
		if (!indices.is_complete(surface_support_required, distinct_compute_queue_required)) {
			continue;
		}

		const int score = _rate_device_suitability(
				dev, required_extensions, p_info.required_features, surface);
		candidates.insert(std::make_pair(score, std::make_pair(dev, indices)));
	}

	QueueFamilyIndices selected_indices;

	// Select the device
	if (candidates.rbegin()->first > 0) {
		std::tie(physical_device, selected_indices) = candidates.rbegin()->second;

		vkGetPhysicalDeviceProperties(physical_device, &physical_device_properties);
		vkGetPhysicalDeviceFeatures(physical_device, &physical_device_features);

		swapchain_supported = _check_device_extension_support(
				physical_device, { VK_KHR_SWAPCHAIN_EXTENSION_NAME });
	} else {
		GL_ASSERT(false, "Failed to find a suitable GPU!");
	}

	// Create the logical device
	std::vector<VkDeviceQueueCreateInfo> queue_create_infos;
	std::set<uint32_t> unique_queue_families = {
		*selected_indices.graphics_family,
		*selected_indices.transfer_family,
	};
	if (selected_indices.present_family) {
		unique_queue_families.insert(*selected_indices.present_family);
	}

	float queue_priority = 1.0f;
	for (uint32_t queue_family : unique_queue_families) {
		VkDeviceQueueCreateInfo queue_create_info = {};
		queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
		queue_create_info.queueFamilyIndex = queue_family;
		queue_create_info.queueCount = 1;
		queue_create_info.pQueuePriorities = &queue_priority;
		queue_create_infos.push_back(queue_create_info);
	}

	// Prepare Features Chain
	VkPhysicalDeviceVulkan13Features features_13 = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
		.synchronization2 = VK_TRUE,
		.dynamicRendering = VK_TRUE,
	};

	VkPhysicalDeviceVulkan12Features features_12 = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
		.pNext = &features_13,
		.descriptorIndexing = VK_TRUE,
		.bufferDeviceAddress = VK_TRUE,
	};

	VkPhysicalDeviceFeatures2 device_features2 = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
        .pNext = &features_12,
        .features = {
            .sampleRateShading = VK_TRUE,
            .samplerAnisotropy = VK_TRUE,
        },
    };

	VkDeviceCreateInfo device_create_info = {};
	device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
	device_create_info.queueCreateInfoCount = static_cast<uint32_t>(queue_create_infos.size());
	device_create_info.pQueueCreateInfos = queue_create_infos.data();
	device_create_info.pNext = &device_features2;

	std::vector<const char*> enabled_extensions = DEVICE_EXTENSIONS_REQUIRED;
	// enable swapchain extension if required or available
	if (swapchain_support_required || swapchain_supported) {
		enabled_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	}
	device_create_info.enabledExtensionCount = static_cast<uint32_t>(enabled_extensions.size());
	device_create_info.ppEnabledExtensionNames = enabled_extensions.data();

	if (vkCreateDevice(physical_device, &device_create_info, nullptr, &device) != VK_SUCCESS) {
		GL_ASSERT(false, "Failed to create logical device!");
		return;
	}

	// Retrieve Queues
	vkGetDeviceQueue(device, selected_indices.graphics_family.value(), 0, &graphics_queue.queue);
	graphics_queue.queue_family = selected_indices.graphics_family.value();

	vkGetDeviceQueue(device, selected_indices.transfer_family.value(), 0, &transfer_queue.queue);
	transfer_queue.queue_family = selected_indices.transfer_family.value();

	vkGetDeviceQueue(device, selected_indices.compute_family.value(), 0, &compute_queue.queue);
	compute_queue.queue_family = selected_indices.compute_family.value();

	vkGetDeviceQueue(device, selected_indices.present_family.value(), 0, &present_queue.queue);
	present_queue.queue_family = selected_indices.present_family.value();

	// Cleanup
	deletion_queue.push_function([this]() {
		if (surface != VK_NULL_HANDLE) {
			vkDestroySurfaceKHR(instance, surface, nullptr);
		}
		vkDestroyDevice(device, nullptr);
#ifdef GL_DEBUG_BUILD
		_destroy_debug_utils_messenger_ext(instance, debug_messenger, nullptr);
#endif
		vkDestroyInstance(instance, nullptr);
	});

	// VMA Setup
	VmaAllocatorCreateInfo allocator_info = {};
	allocator_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	allocator_info.physicalDevice = physical_device;
	allocator_info.device = device;
	allocator_info.instance = instance;
	allocator_info.vulkanApiVersion = VK_API_VERSION_1_3;
	vmaCreateAllocator(&allocator_info, &allocator);

	deletion_queue.push_function([this]() { vmaDestroyAllocator(allocator); });

	// Init commands
	imm_transfer.fence = fence_create();
	imm_transfer.command_pool = command_pool_create((CommandQueue)&transfer_queue);
	imm_transfer.command_buffer = command_pool_allocate(imm_transfer.command_pool);

	imm_graphics.fence = fence_create();
	imm_graphics.command_pool = command_pool_create((CommandQueue)&graphics_queue);
	imm_graphics.command_buffer = command_pool_allocate(imm_graphics.command_pool);

	deletion_queue.push_function([this]() {
		fence_free(imm_transfer.fence);
		command_pool_free(imm_transfer.command_pool);

		fence_free(imm_graphics.fence);
		command_pool_free(imm_graphics.command_pool);
	});

#ifndef GL_DIST_BUILD
	GL_LOG_INFO("[VULKAN] Vulkan Initialized:");
	GL_LOG_INFO("[VULKAN] Device: {}", physical_device_properties.deviceName);
	GL_LOG_INFO("[VULKAN] API: {}.{}.{}", VK_VERSION_MAJOR(physical_device_properties.apiVersion),
			VK_VERSION_MINOR(physical_device_properties.apiVersion),
			VK_VERSION_PATCH(physical_device_properties.apiVersion));
#endif
}

VulkanRenderBackend::~VulkanRenderBackend() {
	deletion_queue.flush();
	s_initialized = false;
}

SurfaceCreateError VulkanRenderBackend::attach_surface(
		void* p_connection_handle, void* p_window_handle) {
	if (!is_swapchain_supported()) {
		return SurfaceCreateError::SWAPCHAIN_NOT_SUPPORTED;
	}

	if (surface != VK_NULL_HANDLE) {
		vkDestroySurfaceKHR(instance, surface, nullptr);
	}

	if (_create_surface_platform_specific(p_connection_handle, p_window_handle)) {
		// TODO: changing surface after device creation might be invalid if the device
		// queue doesn't support the new surface
		return SurfaceCreateError::NONE;
	}

	return SurfaceCreateError::INVALID_COMPOSITOR;
}

bool VulkanRenderBackend::is_swapchain_supported() { return swapchain_supported; }

void VulkanRenderBackend::device_wait() { vkDeviceWaitIdle(device); }

uint32_t VulkanRenderBackend::get_max_msaa_samples() const {
	const VkSampleCountFlags counts =
			physical_device_properties.limits.framebufferColorSampleCounts &
			physical_device_properties.limits.framebufferDepthSampleCounts;

	if (counts & VK_SAMPLE_COUNT_64_BIT) {
		return 64;
	} else if (counts & VK_SAMPLE_COUNT_32_BIT) {
		return 32;
	} else if (counts & VK_SAMPLE_COUNT_16_BIT) {
		return 16;
	} else if (counts & VK_SAMPLE_COUNT_8_BIT) {
		return 8;
	} else if (counts & VK_SAMPLE_COUNT_4_BIT) {
		return 4;
	} else if (counts & VK_SAMPLE_COUNT_2_BIT) {
		return 2;
	}

	return 1;
}

bool VulkanRenderBackend::_check_validation_layer_support() {
	uint32_t layer_count;
	vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

	std::vector<VkLayerProperties> available_layers(layer_count);
	vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

	for (const char* layer_name : VALIDATION_LAYERS) {
		bool layer_found = false;
		for (const auto& layer_properties : available_layers) {
			if (strcmp(layer_name, layer_properties.layerName) == 0) {
				layer_found = true;
				break;
			}
		}
		if (!layer_found) {
			return false;
		}
	}
	return true;
}

uint32_t VulkanRenderBackend::_rate_device_suitability(VkPhysicalDevice p_physical_device,
		const std::vector<const char*>& p_required_extensions,
		RenderBackendFeatureFlags p_required_features, VkSurfaceKHR p_surface) {
	if (!_check_device_extension_support(p_physical_device, p_required_extensions)) {
		return 0;
	}

	bool swapchain_adequate = false;
	if (p_required_features & RENDER_BACKEND_FEATURE_SWAPCHAIN_BIT) {
		if (p_surface) {
			const SurfaceCapabilities capabilities =
					_check_surface_capabilities(p_physical_device, p_surface).value();

			const bool has_sufficient_formats = !capabilities.formats.empty();
			const bool has_sufficient_present_modes = !capabilities.present_modes.empty();

			swapchain_adequate = has_sufficient_present_modes && has_sufficient_formats;
		} else {
			swapchain_adequate = true;
		}
	} else {
		swapchain_adequate = true;
	}

	if (!swapchain_adequate) {
		return 0;
	}

	VkPhysicalDeviceVulkan13Features features_13 = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
	};

	VkPhysicalDeviceVulkan12Features features_12 = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
		.pNext = &features_13,
	};

	VkPhysicalDeviceFeatures2 features = {
		.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2,
		.pNext = &features_12,
	};

	vkGetPhysicalDeviceFeatures2(p_physical_device, &features);

	if (!features_13.dynamicRendering || !features_13.synchronization2 ||
			!features_12.bufferDeviceAddress || !features.features.geometryShader) {
		return 0;
	}

	int score = 0;

	VkPhysicalDeviceProperties properties = {};
	vkGetPhysicalDeviceProperties(p_physical_device, &properties);

	if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
		score += 1000;
	}

	score += properties.limits.maxImageDimension2D;

	return score;
}

VulkanRenderBackend::QueueFamilyIndices VulkanRenderBackend::_find_queue_families(
		VkPhysicalDevice p_device, RenderBackendFeatureFlags p_flags, VkSurfaceKHR p_surface) {
	const bool needs_surface = p_flags & RENDER_BACKEND_FEATURE_ENSURE_SURFACE_SUPPORT;
	const bool distinct_compute_queue = p_flags & RENDER_BACKEND_FEATURE_DISTINCT_COMPUTE_QUEUE_BIT;

	QueueFamilyIndices indices;
	uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(p_device, &queue_family_count, nullptr);
	std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(p_device, &queue_family_count, queue_families.data());

	int i = 0;
	for (const auto& queue_family : queue_families) {
		if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphics_family = i;
		}

		if (queue_family.queueFlags & VK_QUEUE_TRANSFER_BIT) {
			// Prefer a distinct transfer queue if possible, but for now just taking first found
			if (!indices.transfer_family.has_value()) {
				indices.transfer_family = i;
			}
		}

		if (queue_family.queueFlags & VK_QUEUE_COMPUTE_BIT) {
			// Select a distinct queue if required
			if (!distinct_compute_queue ||
					(distinct_compute_queue &&
							!(queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT))) {
				indices.compute_family = i;
			}
		}

		// Ensure surface support if any surface provided
		// Fallback: If swapchain requested, p_required_extensions has to have
		// VK_KHR_SWAPCHAIN_EXTENSION_NAME also swapchain support is already checked.
		if (needs_surface && p_surface != VK_NULL_HANDLE) {
			VkBool32 present_support = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(p_device, i, p_surface, &present_support);
			if (present_support) {
				indices.present_family = i;
			}
		}

		i++;
	}

	return indices;
}

bool VulkanRenderBackend::_check_device_extension_support(
		VkPhysicalDevice p_device, const std::vector<const char*>& p_extensions) {
	uint32_t extension_count;
	vkEnumerateDeviceExtensionProperties(p_device, nullptr, &extension_count, nullptr);
	std::vector<VkExtensionProperties> available_extensions(extension_count);
	vkEnumerateDeviceExtensionProperties(
			p_device, nullptr, &extension_count, available_extensions.data());

	std::set<std::string> required_extensions(p_extensions.begin(), p_extensions.end());
	for (const auto& extension : available_extensions) {
		required_extensions.erase(extension.extensionName);
	}

	return required_extensions.empty();
}

std::optional<VulkanRenderBackend::SurfaceCapabilities>
VulkanRenderBackend::_check_surface_capabilities(
		VkPhysicalDevice p_physical_device, VkSurfaceKHR p_surface) {
	if (p_surface == VK_NULL_HANDLE) {
		return std::nullopt;
	}

	SurfaceCapabilities capabilities;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
			p_physical_device, p_surface, &capabilities.capabilities);

	uint32_t format_count;
	vkGetPhysicalDeviceSurfaceFormatsKHR(p_physical_device, p_surface, &format_count, nullptr);

	if (format_count != 0) {
		capabilities.formats.resize(format_count);
		vkGetPhysicalDeviceSurfaceFormatsKHR(
				p_physical_device, p_surface, &format_count, capabilities.formats.data());
	}

	uint32_t present_mode_count;
	vkGetPhysicalDeviceSurfacePresentModesKHR(
			p_physical_device, p_surface, &present_mode_count, nullptr);

	if (present_mode_count != 0) {
		capabilities.present_modes.resize(present_mode_count);
		vkGetPhysicalDeviceSurfacePresentModesKHR(p_physical_device, p_surface, &present_mode_count,
				capabilities.present_modes.data());
	}

	return capabilities;
}

bool VulkanRenderBackend::_create_surface_platform_specific(void* p_connection, void* p_window) {
	if (!p_window) {
		return false;
	}

#if defined(_WIN32)
	VkWin32SurfaceCreateInfoKHR create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	create_info.hinstance = p_connection ? (HINSTANCE)p_connection : GetModuleHandle(nullptr);
	create_info.hwnd = (HWND)p_window;
	return vkCreateWin32SurfaceKHR(instance, &create_info, nullptr, &surface) == VK_SUCCESS;
#elif defined(__linux__)
	if (!p_connection) {
		return false;
	}

	// TODO: wayland support
	VkXlibSurfaceCreateInfoKHR create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
	create_info.dpy = (Display*)p_connection;
	create_info.window = (Window)p_window;
	return vkCreateXlibSurfaceKHR(instance, &create_info, nullptr, &surface) == VK_SUCCESS;
#else
	return false;
#endif
}

VkResult VulkanRenderBackend::_create_debug_utils_messenger_ext(VkInstance p_instance,
		const VkDebugUtilsMessengerCreateInfoEXT* p_info, const VkAllocationCallbacks* p_allocator,
		VkDebugUtilsMessengerEXT* p_debug_messenger) {
	PFN_vkCreateDebugUtilsMessengerEXT func =
			(PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
					p_instance, "vkCreateDebugUtilsMessengerEXT");
	if (func) {
		return func(p_instance, p_info, p_allocator, p_debug_messenger);
	} else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void VulkanRenderBackend::_destroy_debug_utils_messenger_ext(VkInstance p_instance,
		VkDebugUtilsMessengerEXT p_debug_messenger, const VkAllocationCallbacks* p_allocator) {
	PFN_vkDestroyDebugUtilsMessengerEXT func =
			(PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
					p_instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func) {
		func(p_instance, p_debug_messenger, p_allocator);
	}
}

} //namespace gl
