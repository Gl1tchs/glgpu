/**
 * gpukit_imgui_glue.h
 *
 * Header-only bridge between gpukit and Dear ImGui.
 *
 * Backend selection — define before including this header (or via CMake):
 *   GPUKIT_IMGUI_BACKEND_SDL2     — enables the SDL2 windowing backend
 *   GPUKIT_IMGUI_BACKEND_GLFW     — enables the GLFW windowing backend
 *
 * Usage:
 *   #define GPUKIT_IMGUI_BACKEND_SDL2
 *   #include <imgui.h>
 *   #include <gpukit_imgui_glue.h>
 *
 *   gpukit::ImGuiGlueState imgui = gpukit::imgui_init(info, window);
 *   // per-frame:
 *   gpukit::imgui_new_frame();
 *   ImGui::NewFrame(); // ... your UI ...
 *   gpukit::imgui_render(cmd);
 *   // cleanup:
 *   gpukit::imgui_shutdown(imgui);
 */

#pragma once

#include "gpukit/device.h"

#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

#if defined(GPUKIT_IMGUI_BACKEND_SDL2)
#include <backends/imgui_impl_sdl2.h>
#endif

#if defined(GPUKIT_IMGUI_BACKEND_GLFW)
#include <backends/imgui_impl_glfw.h>
#endif

namespace gpukit {

/**
 * Info required to initialize the rendering backend.
 * Only meaningful when GPUKIT_IMGUI_BACKEND_VULKAN is defined.
 */
struct ImGuiGlueInfo {
	// Number of in-flight frames — must match swapchain image count.
	uint32_t image_count = 2;

	// Swapchain color format. gpukit DataFormat values match VkFormat numerically.
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
 * Initialize all ImGui backends for the active device and native window.
 *
 * Selects rendering backend at compile time:
 *   GPUKIT_IMGUI_BACKEND_VULKAN  → ImGui_ImplVulkan_Init
 *
 * Selects windowing backend at compile time:
 *   GPUKIT_IMGUI_BACKEND_SDL2    → ImGui_ImplSDL2_InitFor*
 *
 * @param info    Rendering backend parameters.
 * @param window  Native window handle (e.g. SDL_Window*). Cast internally.
 * @return Opaque state — pass to imgui_shutdown() on teardown.
 */
inline ImGuiGlueState imgui_init(const ImGuiGlueInfo& info, void* window) {
	ImGuiGlueState state;

	const NativeContext ctx = get_native_context();

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

#if defined(GPUKIT_IMGUI_BACKEND_SDL2)
	{
		auto* sdl_window = static_cast<SDL_Window*>(window);
		ImGui_ImplSDL2_InitForVulkan(sdl_window);
	}
#endif // GPUKIT_IMGUI_BACKEND_SDL2

#if defined(GPUKIT_IMGUI_BACKEND_GLFW)
	{
		auto* glfw_window = static_cast<GLFWwindow*>(window);
		s ImGui_ImplGlfw_InitForVulkan(glfw_window, true);
	}
#endif // GPUKIT_IMGUI_BACKEND_GLFW

	return state;
}

/**
 * Begin a new ImGui frame (rendering + windowing backends).
 * Call once per frame, before ImGui::NewFrame().
 */
inline void imgui_new_frame() {
	ImGui_ImplVulkan_NewFrame();

#if defined(GPUKIT_IMGUI_BACKEND_SDL2)
	ImGui_ImplSDL2_NewFrame();
#endif
#if defined(GPUKIT_IMGUI_BACKEND_GLFW)
	ImGui_ImplGlfw_NewFrame();
#endif
}

/**
 * Record ImGui draw commands into cmd.
 * Must be called inside an active dynamic rendering pass.
 *
 * @param cmd  The gpukit command buffer currently recording.
 */
inline void imgui_render(CommandBuffer cmd) {
	ImGui::Render();
	ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), (VkCommandBuffer)cmd);
}

/**
 * Shut down all ImGui backends and release internal resources.
 * Call before calling gpukit::shutdown().
 *
 * @param state  The state returned by imgui_init().
 */
inline void imgui_shutdown(const ImGuiGlueState& state) {
#if defined(GPUKIT_IMGUI_BACKEND_SDL2)
	ImGui_ImplSDL2_Shutdown();
#endif
#if defined(GPUKIT_IMGUI_BACKEND_GLFW)
	ImGui_ImplGlfw_Shutdown();
#endif

	ImGui_ImplVulkan_Shutdown();
	if (state.pool) {
		const NativeContext ctx = get_native_context();
		vkDestroyDescriptorPool((VkDevice)ctx.device, (VkDescriptorPool)state.pool, nullptr);
	}
}

// Register a gpukit Image+Sampler pair for sampling inside an ImGui::Image() call.
// Assumes the image will be transitioned to SHADER_READ_ONLY_OPTIMAL before ImGui renders.
// Returns an ImTextureID owned by the caller; release it with imgui_remove_texture().
inline ImTextureID imgui_add_texture(Image image, Sampler sampler) {
	auto view_res = image_get_native_view(image);
	auto samp_res = sampler_get_native(sampler);
	if (!view_res || !samp_res)
		return (ImTextureID) nullptr;
	VkDescriptorSet ds = ImGui_ImplVulkan_AddTexture(static_cast<VkSampler>(samp_res.value()),
			static_cast<VkImageView>(view_res.value()), VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
	return (ImTextureID) static_cast<void*>(ds);
}

inline void imgui_remove_texture(ImTextureID texture_id) {
	if (!texture_id)
		return;
	ImGui_ImplVulkan_RemoveTexture((VkDescriptorSet)texture_id);
}

} // namespace gpukit
