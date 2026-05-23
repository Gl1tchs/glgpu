#include <vector>

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

#include <imgui.h>

#include <glgpu/glgpu.h>
#include <glgpu_imgui_glue.h>
#include <glgpu_sdl2_glue.h>

constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;

struct FrameData {
	gl::CommandPool cmd_pool;
	gl::CommandBuffer cmd;
	gl::Semaphore image_available_sem;
	gl::Semaphore render_finished_sem;
	gl::Fence frame_fence;

	void init(gl::Device* device, gl::CommandQueue graphics_queue) {
		cmd_pool = device->command_pool_create(graphics_queue).value();
		cmd = device->command_pool_allocate(cmd_pool).value();
		image_available_sem = device->semaphore_create();
		render_finished_sem = device->semaphore_create();
		frame_fence = device->fence_create();
	}

	void destroy(gl::Device* device) {
		device->fence_free(frame_fence);
		device->semaphore_free(image_available_sem);
		device->semaphore_free(render_finished_sem);
		device->command_pool_free(cmd_pool);
	}
};

int main(void) {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		GL_LOG_ERROR("SDL could not initialize! SDL_Error: {}", SDL_GetError());
		return 1;
	}

	SDL_Window* window = SDL_CreateWindow("GLGPU + ImGui Example", SDL_WINDOWPOS_UNDEFINED,
			SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH, WINDOW_HEIGHT,
			SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);

	if (!window) {
		GL_LOG_ERROR("Window could not be created! SDL_Error: {}", SDL_GetError());
		SDL_Quit();
		return 1;
	}

	// -------------------------------------------------------------------------
	// Device
	// -------------------------------------------------------------------------
	gl::DeviceCreateInfo device_info{
		.required_features = gl::DEVICE_FEATURE_SWAPCHAIN_BIT |
				gl::DEVICE_FEATURE_ENSURE_SURFACE_SUPPORT | gl::DEVICE_FEATURE_VALIDATION_LAYERS,
	};

	if (!gl::extract_sdl2_info(device_info, window)) {
		GL_ASSERT(false, "Failed to extract SDL2 window info.");
	}

	auto device = gl::Device::create(device_info).own();

	gl::CommandQueue graphics_queue = device->queue_get(gl::QueueType::GRAPHICS).value();
	gl::CommandQueue present_queue = device->queue_get(gl::QueueType::PRESENT).value();

	gl::Swapchain swapchain = device->swapchain_create().value();
	device->swapchain_resize(graphics_queue, swapchain, { WINDOW_WIDTH, WINDOW_HEIGHT }, true);

	const uint32_t image_count = device->swapchain_get_image_count(swapchain).value();
	const gl::DataFormat swapchain_format = device->swapchain_get_format(swapchain).value();

	// -------------------------------------------------------------------------
	// Per-frame resources
	// -------------------------------------------------------------------------
	std::vector<FrameData> frames(image_count);
	for (auto& frame : frames) {
		frame.init(device.get(), graphics_queue);
	}

	// -------------------------------------------------------------------------
	// ImGui setup
	// -------------------------------------------------------------------------
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGui::StyleColorsDark();

	// gl::imgui_init handles both the Vulkan and SDL2 backend initialization.
	gl::ImGuiGlueInfo imgui_info{
		.image_count = image_count,
		.color_attachment_format = swapchain_format,
		.min_image_count = image_count,
	};
	gl::ImGuiGlueState imgui = gl::imgui_init(device.get(), imgui_info, (void*)window);

	// -------------------------------------------------------------------------
	// Main loop
	// -------------------------------------------------------------------------
	bool quit = false;
	uint32_t current_frame_index = 0;

	bool show_demo_window = true;
	ImVec4 clear_color = { 0.08f, 0.08f, 0.10f, 1.0f };
	float counter = 0.0f;

	while (!quit) {
		SDL_Event e;
		while (SDL_PollEvent(&e) != 0) {
			ImGui_ImplSDL2_ProcessEvent(&e);

			if (e.type == SDL_QUIT) {
				quit = true;
			}
			if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
				device->device_wait();
				device->swapchain_resize(graphics_queue, swapchain,
						{ (uint32_t)e.window.data1, (uint32_t)e.window.data2 }, true);
			}
		}

		FrameData& frame = frames[current_frame_index];

		device->fence_wait(frame.frame_fence);
		device->fence_reset(frame.frame_fence);

		uint32_t image_index = 0;
		auto acquire_result =
				device->swapchain_acquire_image(swapchain, frame.image_available_sem, &image_index);
		if (!acquire_result) {
			continue;
		}

		gl::Image swapchain_image = *acquire_result;

		// gl::imgui_new_frame calls both ImGui_ImplVulkan_NewFrame and ImGui_ImplSDL2_NewFrame.
		gl::imgui_new_frame(device.get());
		ImGui::NewFrame();

		// --- ImGui UI ---
		if (show_demo_window) {
			ImGui::ShowDemoWindow(&show_demo_window);
		}

		ImGui::Begin("glgpu + ImGui");
		ImGui::Text("Rendering with glgpu's Vulkan backend.");
		ImGui::Separator();
		ImGui::ColorEdit3("Clear color", (float*)&clear_color);
		if (ImGui::Button("Counter++")) {
			counter++;
		}
		ImGui::SameLine();
		ImGui::Text("Count = %.0f", counter);
		ImGui::Checkbox("Show ImGui demo window", &show_demo_window);
		ImGui::Separator();
		ImGui::Text("%.3f ms/frame  (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
		ImGui::End();
		// ----------------

		device->command_reset(frame.cmd);
		device->command_begin(frame.cmd);

		device->command_transition_image(frame.cmd, swapchain_image, gl::ImageLayout::UNDEFINED,
				gl::ImageLayout::COLOR_ATTACHMENT_OPTIMAL);

		gl::RenderingAttachment color_attachment{
			.image = swapchain_image,
			.layout = gl::ImageLayout::COLOR_ATTACHMENT_OPTIMAL,
			.load_op = gl::AttachmentLoadOp::CLEAR,
			.store_op = gl::AttachmentStoreOp::STORE,
			.clear_color = { clear_color.x, clear_color.y, clear_color.z, clear_color.w },
		};

		gl::Vec2u draw_extent = device->swapchain_get_extent(swapchain).value();
		device->command_begin_rendering(frame.cmd, draw_extent, { &color_attachment, 1 });

		gl::imgui_render(device.get(), frame.cmd);

		device->command_end_rendering(frame.cmd);

		device->command_transition_image(frame.cmd, swapchain_image,
				gl::ImageLayout::COLOR_ATTACHMENT_OPTIMAL, gl::ImageLayout::PRESENT_SRC);

		device->command_end(frame.cmd);

		device->queue_submit(graphics_queue, frame.cmd, frame.frame_fence,
				frame.image_available_sem, frame.render_finished_sem);

		device->queue_present(present_queue, swapchain, frame.render_finished_sem);

		current_frame_index = (current_frame_index + 1) % frames.size();
	}

	// -------------------------------------------------------------------------
	// Cleanup
	// -------------------------------------------------------------------------
	device->device_wait();

	// gl::imgui_shutdown handles ImGui_ImplVulkan_Shutdown + ImGui_ImplSDL2_Shutdown.
	gl::imgui_shutdown(device.get(), imgui);
	ImGui::DestroyContext();

	for (auto& frame : frames) {
		frame.destroy(device.get());
	}

	device->swapchain_free(swapchain);

	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
