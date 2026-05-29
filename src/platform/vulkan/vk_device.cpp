#include "platform/vulkan/vk_device.h"

#include "gpukit/os.h"
#include "platform/vulkan/vk_common.h"

#include <map>
#include <set>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

#if defined(_WIN32)
#include <vulkan/vulkan_win32.h>
#include <windows.h>
#elif defined(__ANDROID__)
#include <android/native_window.h>
#include <vulkan/vulkan_android.h>
#elif defined(__linux__)
#include <X11/Xlib.h>
#include <vulkan/vulkan_xlib.h>
#ifdef GPUKIT_HAS_WAYLAND
#include <vulkan/vulkan_wayland.h>
#include <wayland-client.h>
#endif
#endif

#define VMA_IMPLEMENTATION
#include <vk_mem_alloc.h>

namespace gpukit {

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
		GPUKIT_LOG_ERROR("[VULKAN] {}", callback_data->pMessage);
	} else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
		GPUKIT_LOG_WARNING("[VULKAN] {}", callback_data->pMessage);
	} else if (message_severity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) {
		GPUKIT_LOG_INFO("[VULKAN] {}", callback_data->pMessage);
	} else {
		GPUKIT_LOG_TRACE("[VULKAN] {}", callback_data->pMessage);
	}

	return VK_FALSE;
}

Res<> VulkanDevice::init(const DeviceCreateInfo& info) {
	bool use_validation_layers = info.required_features & DEVICE_FEATURE_VALIDATION_LAYERS;
	if (use_validation_layers && !_check_validation_layer_support()) {
		GPUKIT_LOG_WARNING("[VULKAN] Validation layers requested but not available!");
		use_validation_layers = false;
	}

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
	std::vector<const char*> instance_extensions;

	// Add surface extension if requested
	if (info.required_features & DEVICE_FEATURE_ENSURE_SURFACE_SUPPORT ||
			info.native_window_handle != nullptr) {
		instance_extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);
#if defined(_WIN32)
		instance_extensions.push_back(VK_KHR_WIN32_SURFACE_EXTENSION_NAME);
#elif defined(__ANDROID__)
		instance_extensions.push_back(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME);
#elif defined(__linux__)
		switch (get_window_compositor()) {
#ifdef GPUKIT_HAS_WAYLAND
			case WindowCompositor::WAYLAND:
				instance_extensions.push_back(VK_KHR_WAYLAND_SURFACE_EXTENSION_NAME);
				break;
#endif
			case WindowCompositor::X11:
				instance_extensions.push_back(VK_KHR_XLIB_SURFACE_EXTENSION_NAME);
				break;
			default:
				return Error::SURFACE_INVALID_COMPOSITOR;
		}
#endif
	}

	if (use_validation_layers) {
		instance_extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
	}

	instance_info.enabledExtensionCount = static_cast<uint32_t>(instance_extensions.size());
	instance_info.ppEnabledExtensionNames = instance_extensions.data();

	// Validation Layers
	VkDebugUtilsMessengerCreateInfoEXT debug_create_info = {};
	if (use_validation_layers) {
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
	} else {
		instance_info.enabledLayerCount = 0;
		instance_info.pNext = nullptr;
	}

	// Return specific error on failure
	VK_CHECK_RET(
			vkCreateInstance(&instance_info, nullptr, &_instance), Error::INITIALIZATION_FAILED);

	if (use_validation_layers &&
			_create_debug_utils_messenger_ext(
					_instance, &debug_create_info, nullptr, &_debug_messenger) != VK_SUCCESS) {
		GPUKIT_LOG_WARNING("[VULKAN] Failed to set up debug messenger!");
	}

	_vkCmdBeginDebugUtilsLabelEXT = (PFN_vkCmdBeginDebugUtilsLabelEXT)vkGetInstanceProcAddr(
			_instance, "vkCmdBeginDebugUtilsLabelEXT");
	_vkCmdEndDebugUtilsLabelEXT = (PFN_vkCmdEndDebugUtilsLabelEXT)vkGetInstanceProcAddr(
			_instance, "vkCmdEndDebugUtilsLabelEXT");
	_vkSetDebugUtilsObjectNameEXT = (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(
			_instance, "vkSetDebugUtilsObjectNameEXT");

	const bool swapchain_support_required = info.required_features & DEVICE_FEATURE_SWAPCHAIN_BIT;
	const bool surface_support_required =
			info.required_features & DEVICE_FEATURE_ENSURE_SURFACE_SUPPORT;
	const bool distinct_compute_queue_required =
			(info.required_features & DEVICE_FEATURE_DISTINCT_COMPUTE_QUEUE_BIT);

	if (!swapchain_support_required && !surface_support_required) {
		override_window_compositor(WindowCompositor::HEADLESS);
	}

	// Try to create a surface
	if (surface_support_required && !info.native_window_handle) {
		GPUKIT_LOG_ERROR("Surface support required but no window provided.");
		return Error::INVALID_ARGUMENT;
	}

	if ((swapchain_support_required || surface_support_required) && info.native_window_handle) {
		if (!_create_surface_platform_specific(
					info.native_connection_handle, info.native_window_handle)) {
			return Error::WINDOW_CREATION_FAILED;
		}
	}

	// Pick GPU
	uint32_t device_count = 0;
	vkEnumeratePhysicalDevices(_instance, &device_count, nullptr);
	if (device_count == 0) {
		return Error::DEVICE_LOST; // No GPU found
	}
	std::vector<VkPhysicalDevice> devices(device_count);
	vkEnumeratePhysicalDevices(_instance, &device_count, devices.data());

	std::vector<const char*> required_extensions(
			DEVICE_EXTENSIONS_REQUIRED.begin(), DEVICE_EXTENSIONS_REQUIRED.end());

	// Add swapchain extension if requested
	if (swapchain_support_required || surface_support_required) {
		required_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	}

	std::multimap<int, std::pair<VkPhysicalDevice, QueueFamilyIndices>> candidates;
	for (const auto& dev : devices) {
		const QueueFamilyIndices indices =
				_find_queue_families(dev, info.required_features, _surface);
		if (!indices.is_complete(surface_support_required, distinct_compute_queue_required)) {
			continue;
		}

		const int score = _rate_device_suitability(
				dev, required_extensions, info.required_features, _surface);
		candidates.insert(std::make_pair(score, std::make_pair(dev, indices)));
	}

	QueueFamilyIndices selected_indices;

	// Select the device
	if (!candidates.empty() && candidates.rbegin()->first > 0) {
		std::tie(_physical_device, selected_indices) = candidates.rbegin()->second;

		vkGetPhysicalDeviceProperties(_physical_device, &_physical_device_properties);
		vkGetPhysicalDeviceFeatures(_physical_device, &_physical_device_features);

		_swapchain_supported = _check_device_extension_support(
				_physical_device, { VK_KHR_SWAPCHAIN_EXTENSION_NAME });
	} else {
		return Error::DEVICE_LOST; // Failed to find suitable GPU
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
	if (selected_indices.compute_family) {
		unique_queue_families.insert(*selected_indices.compute_family);
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
	VkPhysicalDeviceVulkan13Features features_13 = {};
	features_13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
	features_13.shaderDemoteToHelperInvocation = VK_TRUE;
	features_13.synchronization2 = VK_TRUE;
	features_13.dynamicRendering = VK_TRUE;

	VkPhysicalDeviceVulkan12Features features_12 = {};
	features_12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
	features_12.pNext = &features_13;
	features_12.descriptorIndexing = VK_TRUE;
	features_12.shaderInputAttachmentArrayNonUniformIndexing = VK_TRUE;
	features_12.shaderUniformBufferArrayNonUniformIndexing = VK_TRUE;
	features_12.shaderSampledImageArrayNonUniformIndexing = VK_TRUE;
	features_12.shaderStorageBufferArrayNonUniformIndexing = VK_TRUE;
	features_12.shaderStorageImageArrayNonUniformIndexing = VK_TRUE;
	features_12.descriptorBindingUniformBufferUpdateAfterBind = VK_TRUE;
	features_12.descriptorBindingSampledImageUpdateAfterBind = VK_TRUE;
	features_12.descriptorBindingStorageBufferUpdateAfterBind = VK_TRUE;
	features_12.descriptorBindingStorageImageUpdateAfterBind = VK_TRUE;
	features_12.descriptorBindingPartiallyBound = VK_TRUE;
	features_12.runtimeDescriptorArray = VK_TRUE;
	features_12.bufferDeviceAddress = VK_TRUE;
	features_12.timelineSemaphore = VK_TRUE;

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
	if (swapchain_support_required || _swapchain_supported) {
		enabled_extensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
	}
	device_create_info.enabledExtensionCount = static_cast<uint32_t>(enabled_extensions.size());
	device_create_info.ppEnabledExtensionNames = enabled_extensions.data();

	VK_CHECK_RET(vkCreateDevice(_physical_device, &device_create_info, nullptr, &_device),
			Error::INITIALIZATION_FAILED);

	// Retrieve Queues
	vkGetDeviceQueue(_device, selected_indices.graphics_family.value(), 0, &_graphics_queue.queue);
	_graphics_queue.queue_family = selected_indices.graphics_family.value();
	_graphics_queue.mutex = std::make_shared<std::mutex>();

	vkGetDeviceQueue(_device, selected_indices.transfer_family.value(), 0, &_transfer_queue.queue);
	_transfer_queue.queue_family = selected_indices.transfer_family.value();
	if (_transfer_queue.queue == _graphics_queue.queue) {
		_transfer_queue.mutex = _graphics_queue.mutex;
	} else {
		_transfer_queue.mutex = std::make_shared<std::mutex>();
	}

	if (selected_indices.compute_family) {
		vkGetDeviceQueue(_device, *selected_indices.compute_family, 0, &_compute_queue.queue);
		_compute_queue.queue_family = *selected_indices.compute_family;
	} else {
		_compute_queue.queue = _graphics_queue.queue;
		_compute_queue.queue_family = _graphics_queue.queue_family;
	}

	if (_compute_queue.queue == _graphics_queue.queue) {
		_compute_queue.mutex = _graphics_queue.mutex;
	} else if (_compute_queue.queue == _transfer_queue.queue) {
		_compute_queue.mutex = _transfer_queue.mutex;
	} else {
		_compute_queue.mutex = std::make_shared<std::mutex>();
	}

	if (selected_indices.present_family) {
		vkGetDeviceQueue(_device, *selected_indices.present_family, 0, &_present_queue.queue);
		_present_queue.queue_family = *selected_indices.present_family;
	} else {
		_present_queue.queue = _graphics_queue.queue;
		_present_queue.queue_family = _graphics_queue.queue_family;
	}

	if (_present_queue.queue == _graphics_queue.queue) {
		_present_queue.mutex = _graphics_queue.mutex;
	} else if (_present_queue.queue == _transfer_queue.queue) {
		_present_queue.mutex = _transfer_queue.mutex;
	} else if (_present_queue.queue == _compute_queue.queue) {
		_present_queue.mutex = _compute_queue.mutex;
	} else {
		_present_queue.mutex = std::make_shared<std::mutex>();
	}

	// Cleanup
	_deletion_queue.push_function([this]() {
		if (_surface != VK_NULL_HANDLE) {
			vkDestroySurfaceKHR(_instance, _surface, nullptr);
		}
		vkDestroyDevice(_device, nullptr);

		// Destroy debug messenger if created
		if (_debug_messenger) {
			_destroy_debug_utils_messenger_ext(_instance, _debug_messenger, nullptr);
		}

		vkDestroyInstance(_instance, nullptr);
	});

	// VMA Setup
	VmaAllocatorCreateInfo allocator_info = {};
	allocator_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	allocator_info.physicalDevice = _physical_device;
	allocator_info.device = _device;
	allocator_info.instance = _instance;
	allocator_info.vulkanApiVersion = VK_API_VERSION_1_3;

	// Check VMA creation
	VK_CHECK_RET(vmaCreateAllocator(&allocator_info, &_allocator), Error::OUT_OF_HOST_MEMORY);

	_deletion_queue.push_function([this]() {
#if 0
        // Print VMA Stats
        char* stats_str;
        vmaBuildStatsString(_allocator, &stats_str, true);
        GPUKIT_LOG_TRACE("[VMA] Stats: {}", stats_str);
        vmaFreeStatsString(_allocator, stats_str);
#endif
		// Destroy custom allocation pools
		while (_small_allocs_pools.size()) {
			std::unordered_map<uint32_t, VmaPool>::iterator e = _small_allocs_pools.begin();
			vmaDestroyPool(_allocator, e->second);
			_small_allocs_pools.erase(e);
		}

		vmaDestroyAllocator(_allocator);
	});

	// Init commands (Assuming factory methods for Res types work here)
	// Note: Since we are inside the class, we need to handle the Result<> unwrapping
	// for these internal initializations.

	auto transfer_pool_res = command_pool_create((CommandQueue)&_transfer_queue);
	if (transfer_pool_res.is_error()) {
		return transfer_pool_res.error();
	}

	_imm_transfer.command_pool = transfer_pool_res.value();

	// Fence
	_imm_transfer.fence = fence_create();

	auto transfer_buf_res = command_pool_allocate(_imm_transfer.command_pool);
	if (transfer_buf_res.is_error()) {
		return transfer_buf_res.error();
	}

	_imm_transfer.command_buffer = transfer_buf_res.value();

	// Graphics Immediate
	auto graphics_pool_res = command_pool_create((CommandQueue)&_graphics_queue);
	if (graphics_pool_res.is_error()) {
		return graphics_pool_res.error();
	}

	_imm_graphics.command_pool = graphics_pool_res.value();
	_imm_graphics.fence = fence_create();

	auto graphics_buf_res = command_pool_allocate(_imm_graphics.command_pool);
	if (graphics_buf_res.is_error())
		return graphics_buf_res.error();
	_imm_graphics.command_buffer = graphics_buf_res.value();

	_deletion_queue.push_function([this]() {
		fence_free(_imm_transfer.fence);
		command_pool_free(_imm_transfer.command_pool);

		fence_free(_imm_graphics.fence);
		command_pool_free(_imm_graphics.command_pool);
	});

#ifndef GPUKIT_DIST_BUILD
	GPUKIT_LOG_INFO("[VULKAN] Vulkan Initialized:");
	GPUKIT_LOG_INFO("[VULKAN] Device: {}", _physical_device_properties.deviceName);
	GPUKIT_LOG_INFO("[VULKAN] API: {}.{}.{}",
			VK_VERSION_MAJOR(_physical_device_properties.apiVersion),
			VK_VERSION_MINOR(_physical_device_properties.apiVersion),
			VK_VERSION_PATCH(_physical_device_properties.apiVersion));
#endif

	_max_bindless_descriptors = info.max_bindless_descriptors;
	if (info.pipeline_cache_path) {
		_pipeline_cache_path = info.pipeline_cache_path;
	}

	_pipeline_cache = _load_pipeline_cache();
	_deletion_queue.push_function([this]() { _save_and_destroy_pipeline_cache(); });

	return {};
}

VulkanDevice::~VulkanDevice() { _deletion_queue.flush(); }

Res<> VulkanDevice::attach_surface(void* connection_handle, void* window_handle) {
	if (!is_swapchain_supported()) {
		return Error::SURFACE_SWAPCHAIN_NOT_SUPPORTED;
	}

	if (_surface != VK_NULL_HANDLE) {
		vkDestroySurfaceKHR(_instance, _surface, nullptr);
	}

	if (_create_surface_platform_specific(connection_handle, window_handle)) {
		return {}; // Success
	}

	return Error::SURFACE_INVALID_COMPOSITOR;
}

Res<> VulkanDevice::device_wait() {
	VK_CHECK_RET(vkDeviceWaitIdle(_device), Error::DEVICE_LOST);
	return {};
}

uint32_t VulkanDevice::get_max_msaa_samples() const {
	const VkSampleCountFlags counts =
			_physical_device_properties.limits.framebufferColorSampleCounts &
			_physical_device_properties.limits.framebufferDepthSampleCounts;

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

uint32_t VulkanDevice::get_max_bindless_instances() const { return _max_bindless_descriptors; }

bool VulkanDevice::is_swapchain_supported() { return _swapchain_supported; }

Res<> VulkanDevice::command_begin_label(CommandBuffer cmd, const char* name, Color color) {
	if (!cmd) {
		return Error::INVALID_HANDLE;
	}

	if (!_vkCmdBeginDebugUtilsLabelEXT) {
		return Error::INVALID_OPERATION;
	}

	VkDebugUtilsLabelEXT info = {};
	info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT;
	info.pLabelName = name;
	info.color[0] = color.r;
	info.color[1] = color.g;
	info.color[2] = color.b;
	info.color[3] = color.a;

	_vkCmdBeginDebugUtilsLabelEXT((VkCommandBuffer)cmd, &info);

	return {};
}

Res<> VulkanDevice::command_end_label(CommandBuffer cmd) {
	if (!cmd) {
		return Error::INVALID_HANDLE;
	}

	if (!_vkCmdEndDebugUtilsLabelEXT) {
		return Error::INVALID_OPERATION;
	}

	_vkCmdEndDebugUtilsLabelEXT((VkCommandBuffer)cmd);

	return {};
}

Res<> VulkanDevice::set_debug_name(ObjectType type, void* handle, const char* name) {
	if (!handle) {
		return Error::INVALID_HANDLE;
	}

	if (!_vkSetDebugUtilsObjectNameEXT) {
		// If validation layers/debug utils are not enabled, just return success (noop)
		return {};
	}

	VkDebugUtilsObjectNameInfoEXT name_info = {};
	name_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
	name_info.pObjectName = name;

	switch (type) {
		case ObjectType::BUFFER: {
			VulkanBuffer* vk_buf = (VulkanBuffer*)handle;
			name_info.objectType = VK_OBJECT_TYPE_BUFFER;
			name_info.objectHandle = (uint64_t)vk_buf->vk_buffer;
			break;
		}
		case ObjectType::IMAGE: {
			VulkanImage* vk_img = (VulkanImage*)handle;
			name_info.objectType = VK_OBJECT_TYPE_IMAGE;
			name_info.objectHandle = (uint64_t)vk_img->vk_image;
			break;
		}
		case ObjectType::SAMPLER: {
			name_info.objectType = VK_OBJECT_TYPE_SAMPLER;
			name_info.objectHandle = (uint64_t)handle;
			break;
		}
		case ObjectType::COMMAND_POOL: {
			name_info.objectType = VK_OBJECT_TYPE_COMMAND_POOL;
			name_info.objectHandle = (uint64_t)handle;
			break;
		}
		case ObjectType::COMMAND_BUFFER: {
			name_info.objectType = VK_OBJECT_TYPE_COMMAND_BUFFER;
			name_info.objectHandle = (uint64_t)handle;
			break;
		}
		case ObjectType::COMMAND_QUEUE: {
			VulkanQueue* vk_q = (VulkanQueue*)handle;
			name_info.objectType = VK_OBJECT_TYPE_QUEUE;
			name_info.objectHandle = (uint64_t)vk_q->queue;
			break;
		}
		case ObjectType::RENDER_PASS: {
			VulkanRenderPass* vk_rp = (VulkanRenderPass*)handle;
			name_info.objectType = VK_OBJECT_TYPE_RENDER_PASS;
			name_info.objectHandle = (uint64_t)vk_rp->vk_render_pass;
			break;
		}
		case ObjectType::FRAMEBUFFER: {
			name_info.objectType = VK_OBJECT_TYPE_FRAMEBUFFER;
			name_info.objectHandle = (uint64_t)handle;
			break;
		}
		case ObjectType::SWAPCHAIN: {
			VulkanSwapchain* vk_sc = (VulkanSwapchain*)handle;
			name_info.objectType = VK_OBJECT_TYPE_SWAPCHAIN_KHR;
			name_info.objectHandle = (uint64_t)vk_sc->vk_swapchain;
			break;
		}
		case ObjectType::PIPELINE: {
			VulkanPipeline* vk_pl = (VulkanPipeline*)handle;
			name_info.objectType = VK_OBJECT_TYPE_PIPELINE;
			name_info.objectHandle = (uint64_t)vk_pl->vk_pipeline;
			break;
		}
		case ObjectType::SHADER: {
			VulkanShader* vk_sh = (VulkanShader*)handle;

			// Pipeline layout
			name_info.objectType = VK_OBJECT_TYPE_PIPELINE_LAYOUT;
			name_info.objectHandle = (uint64_t)vk_sh->pipeline_layout;
			_vkSetDebugUtilsObjectNameEXT(_device, &name_info);

			// Individual shader modules
			std::string base_name = name;
			for (size_t i = 0; i < vk_sh->stage_create_infos.size(); ++i) {
				std::string stage_name = base_name + "_stage_" + std::to_string(i);
				VkDebugUtilsObjectNameInfoEXT stage_info = {};
				stage_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT;
				stage_info.objectType = VK_OBJECT_TYPE_SHADER_MODULE;
				stage_info.objectHandle = (uint64_t)vk_sh->stage_create_infos[i].module;
				stage_info.pObjectName = stage_name.c_str();
				_vkSetDebugUtilsObjectNameEXT(_device, &stage_info);
			}
			return {};
		}
		case ObjectType::UNIFORM_SET: {
			VulkanUniformSet* vk_us = (VulkanUniformSet*)handle;
			name_info.objectType = VK_OBJECT_TYPE_DESCRIPTOR_SET;
			name_info.objectHandle = (uint64_t)vk_us->vk_descriptor_set;
			break;
		}
		case ObjectType::FENCE: {
			name_info.objectType = VK_OBJECT_TYPE_FENCE;
			name_info.objectHandle = (uint64_t)handle;
			break;
		}
		case ObjectType::SEMAPHORE: {
			name_info.objectType = VK_OBJECT_TYPE_SEMAPHORE;
			name_info.objectHandle = (uint64_t)handle;
			break;
		}
		default:
			return Error::INVALID_ARGUMENT;
	}

	VK_CHECK_RET(_vkSetDebugUtilsObjectNameEXT(_device, &name_info), Error::INVALID_OPERATION);
	return {};
}

bool VulkanDevice::_check_validation_layer_support() {
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

uint32_t VulkanDevice::_rate_device_suitability(VkPhysicalDevice physical_device,
		const std::vector<const char*>& required_extensions, DeviceFeatureFlags required_features,
		VkSurfaceKHR surface) {
	if (!_check_device_extension_support(physical_device, required_extensions)) {
		return 0;
	}

	bool swapchain_adequate = false;
	if (required_features & DEVICE_FEATURE_SWAPCHAIN_BIT) {
		if (surface) {
			auto cap_res = _check_surface_capabilities(physical_device, surface);
			if (cap_res.has_value()) {
				const SurfaceCapabilities capabilities = cap_res.value();
				const bool has_sufficient_formats = !capabilities.formats.empty();
				const bool has_sufficient_present_modes = !capabilities.present_modes.empty();
				swapchain_adequate = has_sufficient_present_modes && has_sufficient_formats;
			}
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

	vkGetPhysicalDeviceFeatures2(physical_device, &features);

	if (!features_13.shaderDemoteToHelperInvocation || !features_13.dynamicRendering ||
			!features_13.synchronization2 || !features_12.bufferDeviceAddress ||
			!features.features.geometryShader || !features_12.descriptorIndexing ||
			!features_12.shaderSampledImageArrayNonUniformIndexing ||
			!features_12.descriptorBindingSampledImageUpdateAfterBind ||
			!features_12.descriptorBindingPartiallyBound || !features_12.runtimeDescriptorArray) {
		return 0;
	}

	int score = 0;

	VkPhysicalDeviceProperties properties = {};
	vkGetPhysicalDeviceProperties(physical_device, &properties);

	if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
		score += 1000;
	}

	score += properties.limits.maxImageDimension2D;

	return score;
}

VulkanDevice::QueueFamilyIndices VulkanDevice::_find_queue_families(
		VkPhysicalDevice device, DeviceFeatureFlags flags, VkSurfaceKHR surface) {
	const bool needs_surface = flags & DEVICE_FEATURE_ENSURE_SURFACE_SUPPORT;
	const bool distinct_compute_queue = flags & DEVICE_FEATURE_DISTINCT_COMPUTE_QUEUE_BIT;

	QueueFamilyIndices indices;
	uint32_t queue_family_count = 0;
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, nullptr);
	std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
	vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families.data());

	int i = 0;
	for (const auto& queue_family : queue_families) {
		if (queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
			indices.graphics_family = i;
		}

		if (queue_family.queueFlags & VK_QUEUE_TRANSFER_BIT) {
			// Prefer a distinct transfer queue if possible
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

		if (needs_surface && surface != VK_NULL_HANDLE) {
			VkBool32 present_support = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support);
			if (present_support) {
				indices.present_family = i;
			}
		}

		i++;
	}

	return indices;
}

bool VulkanDevice::_check_device_extension_support(
		VkPhysicalDevice device, const std::vector<const char*>& extensions) {
	uint32_t extension_count;
	vkEnumerateDeviceExtensionProperties(device, nullptr, &extension_count, nullptr);
	std::vector<VkExtensionProperties> available_extensions(extension_count);
	vkEnumerateDeviceExtensionProperties(
			device, nullptr, &extension_count, available_extensions.data());

	std::set<std::string> required_extensions(extensions.begin(), extensions.end());
	for (const auto& extension : available_extensions) {
		required_extensions.erase(extension.extensionName);
	}

	return required_extensions.empty();
}

std::optional<VulkanDevice::SurfaceCapabilities> VulkanDevice::_check_surface_capabilities(
		VkPhysicalDevice physical_device, VkSurfaceKHR surface) {
	if (surface == VK_NULL_HANDLE) {
		return std::nullopt;
	}

	SurfaceCapabilities capabilities;

	vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical_device, surface, &capabilities.capabilities);

	uint32_t format_count;
	vkGetPhysicalDeviceSurfaceFormatsKHR(physical_device, surface, &format_count, nullptr);

	if (format_count != 0) {
		capabilities.formats.resize(format_count);
		vkGetPhysicalDeviceSurfaceFormatsKHR(
				physical_device, surface, &format_count, capabilities.formats.data());
	}

	uint32_t present_mode_count;
	vkGetPhysicalDeviceSurfacePresentModesKHR(
			physical_device, surface, &present_mode_count, nullptr);

	if (present_mode_count != 0) {
		capabilities.present_modes.resize(present_mode_count);
		vkGetPhysicalDeviceSurfacePresentModesKHR(
				physical_device, surface, &present_mode_count, capabilities.present_modes.data());
	}

	return capabilities;
}

bool VulkanDevice::_create_surface_platform_specific(void* connection, void* window) {
	if (!window) {
		return false;
	}

#if defined(_WIN32)
	VkWin32SurfaceCreateInfoKHR create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
	create_info.hinstance = connection ? (HINSTANCE)connection : GetModuleHandle(nullptr);
	create_info.hwnd = (HWND)window;
	return vkCreateWin32SurfaceKHR(_instance, &create_info, nullptr, &_surface) == VK_SUCCESS;
#elif defined(__ANDROID__)
	VkAndroidSurfaceCreateInfoKHR create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
	create_info.window = static_cast<ANativeWindow*>(window);
	return vkCreateAndroidSurfaceKHR(_instance, &create_info, nullptr, &_surface) == VK_SUCCESS;
#elif defined(__linux__)
	if (!connection) {
		return false;
	}

	switch (get_window_compositor()) {
#ifdef GPUKIT_HAS_WAYLAND
		case WindowCompositor::WAYLAND: {
			VkWaylandSurfaceCreateInfoKHR create_info = {};
			create_info.sType = VK_STRUCTURE_TYPE_WAYLAND_SURFACE_CREATE_INFO_KHR;
			create_info.display = (wl_display*)connection;
			create_info.surface = (wl_surface*)window;
			return vkCreateWaylandSurfaceKHR(_instance, &create_info, nullptr, &_surface) ==
					VK_SUCCESS;
		}
#endif
		case WindowCompositor::X11: {
			VkXlibSurfaceCreateInfoKHR create_info = {};
			create_info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
			create_info.dpy = (Display*)connection;
			create_info.window = (Window)(uintptr_t)window;
			return vkCreateXlibSurfaceKHR(_instance, &create_info, nullptr, &_surface) ==
					VK_SUCCESS;
		}
		default: {
			GPUKIT_LOG_ERROR("[GPUKit] Invalid compositor");
			return false;
		}
	}
#else
	return false;
#endif
}

VkResult VulkanDevice::_create_debug_utils_messenger_ext(VkInstance instance,
		const VkDebugUtilsMessengerCreateInfoEXT* info, const VkAllocationCallbacks* allocator,
		VkDebugUtilsMessengerEXT* debug_messenger) {
	PFN_vkCreateDebugUtilsMessengerEXT func =
			(PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
					instance, "vkCreateDebugUtilsMessengerEXT");
	if (func) {
		return func(instance, info, allocator, debug_messenger);
	} else {
		return VK_ERROR_EXTENSION_NOT_PRESENT;
	}
}

void VulkanDevice::_destroy_debug_utils_messenger_ext(VkInstance instance,
		VkDebugUtilsMessengerEXT debug_messenger, const VkAllocationCallbacks* allocator) {
	PFN_vkDestroyDebugUtilsMessengerEXT func =
			(PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
					instance, "vkDestroyDebugUtilsMessengerEXT");
	if (func) {
		func(instance, debug_messenger, allocator);
	}
}

NativeContext VulkanDevice::get_native_context() const {
	return {
		.instance = (void*)_instance,
		.physical_device = (void*)_physical_device,
		.device = (void*)_device,
		.graphics_queue = (void*)_graphics_queue.queue,
		.graphics_queue_family = _graphics_queue.queue_family,
	};
}

} //namespace gpukit
