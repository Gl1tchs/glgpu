#include "platform/vulkan/vk_backend.h"

#include "glgpu/assert.h"
#include "platform/vulkan/vk_common.h"

#include <VkBootstrap.h>
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

inline static VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
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

	s_initialized = true;

	vkb::InstanceBuilder vkb_builder;
	const auto vkb_instance_result = vkb_builder
											 .set_app_name("glitch")
#ifdef GL_DEBUG_BUILD
											 .enable_validation_layers()
											 .set_debug_callback(_vk_debug_callback)
#endif
											 .require_api_version(1, 3, 0)
											 .set_headless(p_info.headless_mode)
											 .build();

	if (!vkb_instance_result.has_value()) {
		GL_ASSERT(false, "Unable to create Vulkan instance of version 1.3.0!");
		return;
	}

	vkb::Instance vkb_instance = vkb_instance_result.value();

	instance = vkb_instance.instance;
	debug_messenger = vkb_instance.debug_messenger;

	headless_mode = p_info.headless_mode;
	if (!headless_mode) {
#if defined(_WIN32)
		// TODO! needs to be tested
		VkWin32SurfaceCreateInfoKHR create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR;
		// If user didn't provide HINSTANCE, we can get it for the current process
		create_info.hinstance = p_info.native_connection_handle
				? (HINSTANCE)p_info.native_connection_handle
				: GetModuleHandle(nullptr);
		create_info.hwnd = (HWND)p_info.native_window_handle;

		VK_CHECK(vkCreateWin32SurfaceKHR(instance, &create_info, nullptr, &surface));
#elif defined(__linux__)
		VkXlibSurfaceCreateInfoKHR create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_XLIB_SURFACE_CREATE_INFO_KHR;
		create_info.dpy = (Display*)p_info.native_connection_handle;
		create_info.window = (Window)p_info.native_window_handle;

		VK_CHECK(vkCreateXlibSurfaceKHR(instance, &create_info, nullptr, &surface));
#endif
	} else {
		surface = VK_NULL_HANDLE;
	}

	vkb::PhysicalDeviceSelector vkb_device_selector =
			vkb::PhysicalDeviceSelector(vkb_instance)
					.set_minimum_version(1, 3)
					.set_required_features_13({
							.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES,
							.synchronization2 = true,
							.dynamicRendering = !headless_mode, // only require in non-headless mode
					})
					.set_required_features_12({
							.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES,
							.descriptorIndexing = true,
							.bufferDeviceAddress = true,
					})
					.set_required_features({
							.sampleRateShading = true,
							.depthBounds = true,
					});

	if (surface != VK_NULL_HANDLE) {
		vkb_device_selector.set_surface(surface);
		vkb_device_selector.add_required_extensions({
				"VK_KHR_swapchain",
				"VK_KHR_dynamic_rendering",
		});
	}

	const auto physical_device_res = vkb_device_selector.select();
	if (!physical_device_res.has_value()) {
		for (const auto& err : physical_device_res.detailed_failure_reasons()) {
			GL_LOG_ERROR("[VULKAN] {}", err);
		}
		GL_ASSERT(false);
	}

	vkb::PhysicalDevice vkb_physical_device = physical_device_res.value();

	// create the final vulkan device
	vkb::DeviceBuilder vkb_device_builder(vkb_physical_device);
	vkb::Device vkb_device = vkb_device_builder.build().value();

	physical_device = vkb_device.physical_device;

	physical_device_properties = vkb_device.physical_device.properties;
	physical_device_features = vkb_device.physical_device.features;

	device = vkb_device.device;

	graphics_queue.queue = vkb_device.get_queue(vkb::QueueType::graphics).value();
	graphics_queue.queue_family = vkb_device.get_queue_index(vkb::QueueType::graphics).value();

	// Only try to find a present queue if a surface is defined
	if (surface != VK_NULL_HANDLE) {
		if (auto vkb_queue = vkb_device.get_queue(vkb::QueueType::present)) {
			present_queue.queue = vkb_queue.value();
			present_queue.queue_family =
					vkb_device.get_queue_index(vkb::QueueType::present).value();
		} else {
			present_queue.queue = graphics_queue.queue;
			present_queue.queue_family = graphics_queue.queue_family;
		}
	} else {
		present_queue.queue = graphics_queue.queue;
		present_queue.queue_family = graphics_queue.queue_family;
	}

	if (auto vkb_queue = vkb_device.get_queue(vkb::QueueType::transfer)) {
		transfer_queue.queue = vkb_queue.value();
		transfer_queue.queue_family = vkb_device.get_queue_index(vkb::QueueType::transfer).value();
	} else {
		transfer_queue.queue = graphics_queue.queue;
		transfer_queue.queue_family = graphics_queue.queue_family;
	}

	deletion_queue.push_function([this]() {
		// destroy the window surface if exists
		if (surface != VK_NULL_HANDLE) {
			vkDestroySurfaceKHR(instance, surface, nullptr);
		}

		vkDestroyDevice(device, nullptr);

		vkb::destroy_debug_utils_messenger(instance, debug_messenger, nullptr);
		vkDestroyInstance(instance, nullptr);
	});

	VmaAllocatorCreateInfo allocator_info = {};
	allocator_info.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
	allocator_info.physicalDevice = vkb_physical_device;
	allocator_info.device = device;
	allocator_info.instance = instance;

	vmaCreateAllocator(&allocator_info, &allocator);

	deletion_queue.push_function([this]() {
		while (small_allocs_pools.size()) {
			std::unordered_map<uint32_t, VmaPool>::iterator e = small_allocs_pools.begin();
			vmaDestroyPool(allocator, e->second);
			small_allocs_pools.erase(e);
		}
		vmaDestroyAllocator(allocator);
	});

#ifndef GL_DIST_BUILD
	// inform user about the chosen device
	GL_LOG_INFO("[VULKAN] Vulkan Initialized:");
	GL_LOG_INFO("[VULKAN] Device: {}", physical_device_properties.deviceName);
	GL_LOG_INFO("[VULKAN] API: {}.{}.{}", VK_VERSION_MAJOR(physical_device_properties.apiVersion),
			VK_VERSION_MINOR(physical_device_properties.apiVersion),
			VK_VERSION_PATCH(physical_device_properties.apiVersion));
#endif

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

	deletion_queue.push_function([this]() {
		for (const auto& pools : descriptor_set_pools) {
			for (const auto& pool : pools.second) {
				vkDestroyDescriptorPool(device, pool.first, nullptr);
			}
		}
	});
}

VulkanRenderBackend::~VulkanRenderBackend() {
	deletion_queue.flush();
	s_initialized = false;
}

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

} //namespace gl