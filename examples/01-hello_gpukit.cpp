#include "gpukit/assert.h"
#include <vector>

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

#include <gpukit/gpukit.h>
#include <gpukit_sdl2_glue.h>

constexpr int WINDOW_WIDTH = 800;
constexpr int WINDOW_HEIGHT = 600;

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

	SDL_Window* window = SDL_CreateWindow("GPUKit Clear Screen Test", SDL_WINDOWPOS_UNDEFINED,
			SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH, WINDOW_HEIGHT,
			SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

	if (window == nullptr) {
		GPUKIT_LOG_ERROR("Window could not be created! SDL_Error: {}", SDL_GetError());
		SDL_Quit();
		return 1;
	}

	gpukit::DeviceCreateInfo info{
		.required_features = gpukit::DEVICE_FEATURE_SWAPCHAIN_BIT |
				gpukit::DEVICE_FEATURE_ENSURE_SURFACE_SUPPORT | gpukit::DEVICE_FEATURE_VALIDATION_LAYERS,
	};

	if (!gpukit::extract_sdl2_info(info, window)) {
		GPUKIT_ASSERT(false, "Only X11 and windows is supported.");
	}

	GPUKIT_ASSERT(gpukit::init(info));

	gpukit::CommandQueue graphics_queue = gpukit::queue_get(gpukit::QueueType::GRAPHICS).value();
	gpukit::CommandQueue present_queue = gpukit::queue_get(gpukit::QueueType::PRESENT).value();

	gpukit::Swapchain swapchain = gpukit::swapchain_create().value();
	gpukit::swapchain_resize(graphics_queue, swapchain, { WINDOW_WIDTH, WINDOW_HEIGHT }, true);

	uint32_t image_count = gpukit::swapchain_get_image_count(swapchain).value();
	std::vector<FrameData> frames(image_count);

	for (auto& frame : frames) {
		frame.init(graphics_queue);
	}

	std::vector<gpukit::Semaphore> render_finished_sems(image_count);
	for (uint32_t i = 0; i < image_count; i++) {
		render_finished_sems[i] = gpukit::semaphore_create();
	}

	float time = 0.0f;
	bool quit = false;
	uint32_t current_frame_index = 0;

	while (!quit) {
		SDL_Event e;
		while (SDL_PollEvent(&e) != 0) {
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

				image_count = gpukit::swapchain_get_image_count(swapchain).value();
				frames.resize(image_count);
				for (auto& frame : frames) {
					frame.init(graphics_queue);
				}

				render_finished_sems.resize(image_count);
				for (uint32_t i = 0; i < image_count; i++) {
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

		if (!acquire_result)
			continue;

		gpukit::Image swapchain_image = *acquire_result;

		gpukit::command_reset(frame.cmd);
		gpukit::command_begin(frame.cmd);

		gpukit::command_transition_image(frame.cmd, swapchain_image,
				gpukit::ImageLayout::UNDEFINED, gpukit::ImageLayout::GENERAL);

		gpukit::command_begin_label(frame.cmd, "HELLO WORLD", gpukit::COLOR_RED);

		time += 0.01f;
		gpukit::Color clear_color = { (float)std::abs(sin(time)), (float)std::abs(cos(time)), 0.2f,
			1.0f };

		gpukit::command_clear_color(frame.cmd, swapchain_image, clear_color);

		gpukit::command_end_label(frame.cmd);

		gpukit::command_transition_image(frame.cmd, swapchain_image, gpukit::ImageLayout::GENERAL,
				gpukit::ImageLayout::PRESENT_SRC);

		gpukit::command_end(frame.cmd);

		gpukit::queue_submit(graphics_queue, frame.cmd, frame.frame_fence,
				frame.image_available_sem, render_finished_sems[image_index]);

		gpukit::queue_present(present_queue, swapchain, render_finished_sems[image_index]);

		current_frame_index = (current_frame_index + 1) % frames.size();
	}

	gpukit::device_wait();

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
