#include <vector>

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

#include <imgui.h>

#include <gpukit/gpukit.h>
#include <gpukit_imgui_glue.h>
#include <gpukit_sdl2_glue.h>

constexpr int WINDOW_WIDTH = 1280;
constexpr int WINDOW_HEIGHT = 720;

struct FrameData {
	gpukit::CommandPool cmd_pool;
	gpukit::CommandBuffer cmd;
	gpukit::Semaphore image_available_sem;
	gpukit::Fence frame_fence;

	void init(gpukit::Device* device, gpukit::CommandQueue graphics_queue) {
		cmd_pool = device->command_pool_create(graphics_queue).value();
		cmd = device->command_pool_allocate(cmd_pool).value();
		image_available_sem = device->semaphore_create();
		frame_fence = device->fence_create();
	}

	void destroy(gpukit::Device* device) {
		device->fence_free(frame_fence);
		device->semaphore_free(image_available_sem);
		device->command_pool_free(cmd_pool);
	}
};

int main(void) {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		GPUKIT_LOG_ERROR("SDL could not initialize! SDL_Error: {}", SDL_GetError());
		return 1;
	}

	SDL_Window* window = SDL_CreateWindow("GPUKit + ImGui Example", SDL_WINDOWPOS_UNDEFINED,
			SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH, WINDOW_HEIGHT,
			SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE | SDL_WINDOW_VULKAN);

	if (!window) {
		GPUKIT_LOG_ERROR("Window could not be created! SDL_Error: {}", SDL_GetError());
		SDL_Quit();
		return 1;
	}

	// -------------------------------------------------------------------------
	// Device
	// -------------------------------------------------------------------------
	gpukit::DeviceCreateInfo device_info{
		.required_features = gpukit::DEVICE_FEATURE_SWAPCHAIN_BIT |
				gpukit::DEVICE_FEATURE_ENSURE_SURFACE_SUPPORT | gpukit::DEVICE_FEATURE_VALIDATION_LAYERS,
	};

	if (!gpukit::extract_sdl2_info(device_info, window)) {
		GPUKIT_ASSERT(false, "Failed to extract SDL2 window info.");
	}

	auto device = gpukit::Device::create(device_info).own();

	gpukit::CommandQueue graphics_queue = device->queue_get(gpukit::QueueType::GRAPHICS).value();
	gpukit::CommandQueue present_queue = device->queue_get(gpukit::QueueType::PRESENT).value();

	gpukit::Swapchain swapchain = device->swapchain_create().value();
	device->swapchain_resize(graphics_queue, swapchain, { WINDOW_WIDTH, WINDOW_HEIGHT }, true);

	const uint32_t image_count = device->swapchain_get_image_count(swapchain).value();
	const gpukit::DataFormat swapchain_format = device->swapchain_get_format(swapchain).value();

	// -------------------------------------------------------------------------
	// Per-frame resources
	// -------------------------------------------------------------------------
	std::vector<FrameData> frames(image_count);
	for (auto& frame : frames) {
		frame.init(device.get(), graphics_queue);
	}

	std::vector<gpukit::Semaphore> render_finished_sems(image_count);
	for (uint32_t i = 0; i < image_count; i++) {
		render_finished_sems[i] = device->semaphore_create();
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

	// gpukit::imgui_init handles both the Vulkan and SDL2 backend initialization.
	gpukit::ImGuiGlueInfo imgui_info{
		.image_count = image_count,
		.color_attachment_format = swapchain_format,
		.min_image_count = image_count,
	};
	gpukit::ImGuiGlueState imgui = gpukit::imgui_init(device.get(), imgui_info, (void*)window);

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

				// Recreate frames and render_finished_sems in case image count changed
				for (auto& frame : frames) {
					frame.destroy(device.get());
				}
				for (auto sem : render_finished_sems) {
					device->semaphore_free(sem);
				}

				uint32_t new_image_count = device->swapchain_get_image_count(swapchain).value();
				frames.resize(new_image_count);
				for (auto& frame : frames) {
					frame.init(device.get(), graphics_queue);
				}

				render_finished_sems.resize(new_image_count);
				for (uint32_t i = 0; i < new_image_count; i++) {
					render_finished_sems[i] = device->semaphore_create();
				}

				current_frame_index = 0;
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

		gpukit::Image swapchain_image = *acquire_result;

		// gpukit::imgui_new_frame calls both ImGui_ImplVulkan_NewFrame and ImGui_ImplSDL2_NewFrame.
		gpukit::imgui_new_frame(device.get());
		ImGui::NewFrame();

		// --- ImGui UI ---
		if (show_demo_window) {
			ImGui::ShowDemoWindow(&show_demo_window);
		}

		ImGui::Begin("gpukit + ImGui");
		ImGui::Text("Rendering with gpukit's Vulkan backend.");
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

		device->command_transition_image(frame.cmd, swapchain_image, gpukit::ImageLayout::UNDEFINED,
				gpukit::ImageLayout::COLOR_ATTACHMENT_OPTIMAL);

		gpukit::RenderingAttachment color_attachment{
			.image = swapchain_image,
			.layout = gpukit::ImageLayout::COLOR_ATTACHMENT_OPTIMAL,
			.load_op = gpukit::AttachmentLoadOp::CLEAR,
			.store_op = gpukit::AttachmentStoreOp::STORE,
			.clear_color = { clear_color.x, clear_color.y, clear_color.z, clear_color.w },
		};

		gpukit::Vec2u draw_extent = device->swapchain_get_extent(swapchain).value();
		device->command_begin_rendering(frame.cmd, draw_extent, { &color_attachment, 1 });

		gpukit::imgui_render(device.get(), frame.cmd);

		device->command_end_rendering(frame.cmd);

		device->command_transition_image(frame.cmd, swapchain_image,
				gpukit::ImageLayout::COLOR_ATTACHMENT_OPTIMAL, gpukit::ImageLayout::PRESENT_SRC);

		device->command_end(frame.cmd);

		device->queue_submit(graphics_queue, frame.cmd, frame.frame_fence,
				frame.image_available_sem, render_finished_sems[image_index]);

		device->queue_present(present_queue, swapchain, render_finished_sems[image_index]);

		current_frame_index = (current_frame_index + 1) % frames.size();
	}

	// -------------------------------------------------------------------------
	// Cleanup
	// -------------------------------------------------------------------------
	device->device_wait();

	// gpukit::imgui_shutdown handles ImGui_ImplVulkan_Shutdown + ImGui_ImplSDL2_Shutdown.
	gpukit::imgui_shutdown(device.get(), imgui);
	ImGui::DestroyContext();

	for (auto& frame : frames) {
		frame.destroy(device.get());
	}

	for (auto sem : render_finished_sems) {
		device->semaphore_free(sem);
	}

	device->swapchain_free(swapchain);

	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
