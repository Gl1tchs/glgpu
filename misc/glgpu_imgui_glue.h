/**
 * glgpu_imgui_glue.h
 *
 * Header-only bridge between glgpu and Dear ImGui.
 *
 * Backend selection — define before including this header (or via CMake):
 *   GLGPU_IMGUI_BACKEND_VULKAN   — enables the Vulkan rendering backend
 *   GLGPU_IMGUI_BACKEND_SDL2     — enables the SDL2 windowing backend
 *
 * Usage:
 *   #define GLGPU_IMGUI_BACKEND_VULKAN
 *   #define GLGPU_IMGUI_BACKEND_SDL2
 *   #include <imgui.h>
 *   #include <glgpu_imgui_glue.h>
 *
 *   gl::ImGuiGlueState imgui = gl::imgui_init(device.get(), info, window);
 *   // per-frame:
 *   gl::imgui_new_frame();
 *   ImGui::NewFrame(); // ... your UI ...
 *   gl::imgui_render(cmd);
 *   // cleanup:
 *   gl::imgui_shutdown(device.get(), imgui);
 */

#pragma once

#include <imgui.h>

#include "glgpu/device.h"

#if defined(GLGPU_IMGUI_BACKEND_VULKAN)
#include <backends/imgui_impl_vulkan.h>
#endif

#if defined(GLGPU_IMGUI_BACKEND_SDL2)
#include <backends/imgui_impl_sdl2.h>
#endif

namespace gl {

/**
 * Info required to initialize the rendering backend.
 * Only meaningful when GLGPU_IMGUI_BACKEND_VULKAN is defined.
 */
struct ImGuiGlueInfo {
	// Number of in-flight frames — must match swapchain image count.
	uint32_t image_count = 2;

	// Swapchain color format. glgpu DataFormat values match VkFormat numerically.
	DataFormat color_attachment_format = DataFormat::B8G8R8A8_UNORM;

	// Minimum image count hint forwarded to ImGui.
	uint32_t min_image_count = 2;
};

/**
 * Opaque handle returned by imgui_init(). Pass to imgui_shutdown().
 * Hides backend-specific resources (e.g. VkDescriptorPool) from caller code.
 */
struct ImGuiGlueState {
	void* pool = nullptr; // Vulkan: VkDescriptorPool
};

/**
 * Initialize all ImGui backends for the given device and native window.
 *
 * Selects rendering backend at compile time:
 *   GLGPU_IMGUI_BACKEND_VULKAN  → ImGui_ImplVulkan_Init
 *
 * Selects windowing backend at compile time:
 *   GLGPU_IMGUI_BACKEND_SDL2    → ImGui_ImplSDL2_InitFor*
 *
 * @param device  The active glgpu device.
 * @param info    Rendering backend parameters.
 * @param window  Native window handle (e.g. SDL_Window*). Cast internally.
 * @return Opaque state — pass to imgui_shutdown() on teardown.
 */
inline ImGuiGlueState imgui_init(Device* device, const ImGuiGlueInfo& info, void* window) {
	ImGuiGlueState state;

	if (device->get_api() == RenderAPI::VULKAN) {
#if defined(GLGPU_IMGUI_BACKEND_VULKAN)
		const NativeContext ctx = device->get_native_context();

		const VkDescriptorPoolSize pool_sizes[] = {
			{ VK_DESCRIPTOR_TYPE_SAMPLER, 100 },
			{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 100 },
		};

		VkDescriptorPoolCreateInfo pool_info = {};
		pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
		pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
		pool_info.maxSets = 200;
		pool_info.poolSizeCount = 2;
		pool_info.pPoolSizes = pool_sizes;

		VkDescriptorPool pool = VK_NULL_HANDLE;
		vkCreateDescriptorPool((VkDevice)ctx.device, &pool_info, nullptr, &pool);
		state.pool = (void*)pool;

		const VkFormat vk_color_format = static_cast<VkFormat>(info.color_attachment_format);

		VkPipelineRenderingCreateInfoKHR pipeline_rendering_info = {};
		pipeline_rendering_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR;
		pipeline_rendering_info.colorAttachmentCount = 1;
		pipeline_rendering_info.pColorAttachmentFormats = &vk_color_format;

		ImGui_ImplVulkan_InitInfo init_info = {};
		init_info.Instance = (VkInstance)ctx.instance;
		init_info.PhysicalDevice = (VkPhysicalDevice)ctx.physical_device;
		init_info.Device = (VkDevice)ctx.device;
		init_info.QueueFamily = ctx.graphics_queue_family;
		init_info.Queue = (VkQueue)ctx.graphics_queue;
		init_info.DescriptorPool = pool;
		init_info.MinImageCount = info.min_image_count;
		init_info.ImageCount = info.image_count;
		init_info.UseDynamicRendering = true;
		init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
		init_info.PipelineInfoMain.PipelineRenderingCreateInfo = pipeline_rendering_info;

		ImGui_ImplVulkan_Init(&init_info);
#endif // GLGPU_IMGUI_BACKEND_VULKAN
	}

#if defined(GLGPU_IMGUI_BACKEND_SDL2)
	{
		auto* sdl_window = static_cast<SDL_Window*>(window);

		if (device->get_api() == RenderAPI::VULKAN) {
#if defined(GLGPU_IMGUI_BACKEND_VULKAN)
			ImGui_ImplSDL2_InitForVulkan(sdl_window);
#else
			ImGui_ImplSDL2_InitForOther(sdl_window);
#endif
		} else {
			ImGui_ImplSDL2_InitForOther(sdl_window);
		}
	}
#endif // GLGPU_IMGUI_BACKEND_SDL2

	return state;
}

/**
 * Begin a new ImGui frame (rendering + windowing backends).
 * Call once per frame, before ImGui::NewFrame().
 */
inline void imgui_new_frame(Device* device) {
	if (device->get_api() == RenderAPI::VULKAN) {
#if defined(GLGPU_IMGUI_BACKEND_VULKAN)
		ImGui_ImplVulkan_NewFrame();
#endif
	}
#if defined(GLGPU_IMGUI_BACKEND_SDL2)
	ImGui_ImplSDL2_NewFrame();
#endif
}

/**
 * Record ImGui draw commands into cmd.
 * Must be called inside an active dynamic rendering pass.
 *
 * @param cmd  The glgpu command buffer currently recording.
 */
inline void imgui_render(Device* device, CommandBuffer cmd) {
	ImGui::Render();
	if (device->get_api() == RenderAPI::VULKAN) {
#if defined(GLGPU_IMGUI_BACKEND_VULKAN)
		ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), (VkCommandBuffer)cmd);
#endif
	}
}

/**
 * Shut down all ImGui backends and release internal resources.
 * Call before destroying the glgpu device.
 *
 * @param device  The active glgpu device.
 * @param state   The state returned by imgui_init().
 */
inline void imgui_shutdown(Device* device, const ImGuiGlueState& state) {
#if defined(GLGPU_IMGUI_BACKEND_SDL2)
	ImGui_ImplSDL2_Shutdown();
#endif
	if (device->get_api() == RenderAPI::VULKAN) {
#if defined(GLGPU_IMGUI_BACKEND_VULKAN)
		ImGui_ImplVulkan_Shutdown();
		if (state.pool) {
			const NativeContext ctx = device->get_native_context();
			vkDestroyDescriptorPool((VkDevice)ctx.device, (VkDescriptorPool)state.pool, nullptr);
		}
#endif
	}
}

// ---------------------------------------------------------------------------
// Legacy aliases
// ---------------------------------------------------------------------------

using ImGuiVulkanGlueInfo = ImGuiGlueInfo;

inline bool imgui_vulkan_init(
		Device* device, const ImGuiGlueInfo& info, void* imgui_pool_out, void* window) {
	// imgui_pool_out unused; pool is now owned by ImGuiGlueState.
	(void)imgui_pool_out;
	imgui_init(device, info, window);
	return true;
}

inline void imgui_vulkan_new_frame(Device* device) { imgui_new_frame(device); }
inline void imgui_vulkan_render(Device* device, CommandBuffer cmd) { imgui_render(device, cmd); }

} // namespace gl
