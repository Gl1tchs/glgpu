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

	void init(gpukit::CommandQueue graphics_queue) {
		cmd_pool = gpukit::command_pool_create(graphics_queue).value();
		cmd = gpukit::command_pool_allocate(cmd_pool).value();
		image_available_sem = gpukit::semaphore_create();
		frame_fence = gpukit::fence_create();
	}

	void destroy() {
		gpukit::fence_free(frame_fence);
		gpukit::semaphore_free(image_available_sem);
		gpukit::command_pool_free(cmd_pool);
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

	gpukit::DeviceCreateInfo device_info{
		.required_features = gpukit::DEVICE_FEATURE_SWAPCHAIN_BIT |
				gpukit::DEVICE_FEATURE_ENSURE_SURFACE_SUPPORT |
				gpukit::DEVICE_FEATURE_VALIDATION_LAYERS,
	};

	if (!gpukit::extract_sdl2_info(device_info, window)) {
		GPUKIT_ASSERT(false, "Failed to extract SDL2 window info.");
	}

	gpukit::init(device_info);

	gpukit::CommandQueue graphics_queue = gpukit::queue_get(gpukit::QueueType::GRAPHICS).value();
	gpukit::CommandQueue present_queue = gpukit::queue_get(gpukit::QueueType::PRESENT).value();

	gpukit::Swapchain swapchain = gpukit::swapchain_create().value();
	gpukit::swapchain_resize(graphics_queue, swapchain, { WINDOW_WIDTH, WINDOW_HEIGHT }, true);

	const uint32_t image_count = gpukit::swapchain_get_image_count(swapchain).value();
	const gpukit::DataFormat swapchain_format = gpukit::swapchain_get_format(swapchain).value();

	std::vector<FrameData> frames(image_count);
	for (auto& frame : frames) {
		frame.init(graphics_queue);
	}

	std::vector<gpukit::Semaphore> render_finished_sems(image_count);
	for (uint32_t i = 0; i < image_count; i++) {
		render_finished_sems[i] = gpukit::semaphore_create();
	}

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();

	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
	io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

	ImGui::StyleColorsDark();

	gpukit::ImGuiGlueInfo imgui_info{
		.image_count = image_count,
		.color_attachment_format = swapchain_format,
		.min_image_count = image_count,
	};
	gpukit::ImGuiGlueState imgui = gpukit::imgui_init(imgui_info, (void*)window);

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
				gpukit::device_wait();
				gpukit::swapchain_resize(graphics_queue, swapchain,
						{ (uint32_t)e.window.data1, (uint32_t)e.window.data2 }, true);

				for (auto& frame : frames) {
					frame.destroy();
				}
				for (auto sem : render_finished_sems) {
					gpukit::semaphore_free(sem);
				}

				uint32_t new_image_count = gpukit::swapchain_get_image_count(swapchain).value();
				frames.resize(new_image_count);
				for (auto& frame : frames) {
					frame.init(graphics_queue);
				}

				render_finished_sems.resize(new_image_count);
				for (uint32_t i = 0; i < new_image_count; i++) {
					render_finished_sems[i] = gpukit::semaphore_create();
				}

				current_frame_index = 0;
			}
		}

		FrameData& frame = frames[current_frame_index];

		gpukit::fence_wait(frame.frame_fence);
		gpukit::fence_reset(frame.frame_fence);

		uint32_t image_index = 0;
		auto acquire_result =
				gpukit::swapchain_acquire_image(swapchain, frame.image_available_sem, &image_index);
		if (!acquire_result) {
			continue;
		}

		gpukit::Image swapchain_image = *acquire_result;

		gpukit::imgui_new_frame();
		ImGui::NewFrame();

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

		gpukit::command_reset(frame.cmd);
		gpukit::command_begin(frame.cmd);

		gpukit::command_transition_image(frame.cmd, swapchain_image, gpukit::ImageLayout::UNDEFINED,
				gpukit::ImageLayout::COLOR_ATTACHMENT_OPTIMAL);

		gpukit::RenderingAttachment color_attachment{
			.image = swapchain_image,
			.layout = gpukit::ImageLayout::COLOR_ATTACHMENT_OPTIMAL,
			.load_op = gpukit::AttachmentLoadOp::CLEAR,
			.store_op = gpukit::AttachmentStoreOp::STORE,
			.clear_color = { clear_color.x, clear_color.y, clear_color.z, clear_color.w },
		};

		gpukit::Vec2u draw_extent = gpukit::swapchain_get_extent(swapchain).value();
		gpukit::command_begin_rendering(frame.cmd, draw_extent, { &color_attachment, 1 });

		gpukit::imgui_render(frame.cmd);

		gpukit::command_end_rendering(frame.cmd);

		gpukit::command_transition_image(frame.cmd, swapchain_image,
				gpukit::ImageLayout::COLOR_ATTACHMENT_OPTIMAL, gpukit::ImageLayout::PRESENT_SRC);

		gpukit::command_end(frame.cmd);

		gpukit::queue_submit(graphics_queue, frame.cmd, frame.frame_fence,
				frame.image_available_sem, render_finished_sems[image_index]);

		gpukit::queue_present(present_queue, swapchain, render_finished_sems[image_index]);

		current_frame_index = (current_frame_index + 1) % frames.size();
	}

	gpukit::device_wait();

	gpukit::imgui_shutdown(imgui);
	ImGui::DestroyContext();

	for (auto& frame : frames) {
		frame.destroy();
	}

	for (auto sem : render_finished_sems) {
		gpukit::semaphore_free(sem);
	}

	gpukit::swapchain_free(swapchain);
	gpukit::shutdown();

	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
