#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

#include "glgpu/assert.h"
#include "glgpu/backend.h"
#include "glgpu/log.h"

using namespace gl;

constexpr int WINDOW_WIDTH = 800;
constexpr int WINDOW_HEIGHT = 600;

int main(void) {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		GL_LOG_ERROR("SDL could not initialize! SDL_Error: {}", SDL_GetError());
		return 1;
	}

	SDL_Window* window = SDL_CreateWindow("GLGPU Clear Screen Test", SDL_WINDOWPOS_UNDEFINED,
			SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH, WINDOW_HEIGHT,
			SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

	if (window == nullptr) {
		GL_LOG_ERROR("Window could not be created! SDL_Error: {}", SDL_GetError());
		SDL_Quit();
		return 1;
	}

	RenderBackendCreateInfo info{
		.features = RENDER_BACKEND_FEATURE_SWAPCHAIN_BIT,
	};

	SDL_SysWMinfo wm_info;
	SDL_VERSION(&wm_info.version);
	if (SDL_GetWindowWMInfo(window, &wm_info)) {
		if (wm_info.subsystem == SDL_SYSWM_X11) {
#ifdef __linux
			info.native_connection_handle = wm_info.info.x11.display;
			info.native_window_handle = (void*)wm_info.info.x11.window;
#endif
		} else if (wm_info.subsystem == SDL_SYSWM_WINDOWS) {
#ifdef _WIN32
			info.native_window_handle = (void*)wm_info.info.win.window;
			info.native_connection_handle = (void*)wm_info.info.win.hinstance; // Usually
#endif
		} else {
			GL_ASSERT(false, "Only X11 and windows is supported.");
		}
	}

	auto backend = RenderBackend::create(info);

	CommandQueue graphics_queue = backend->queue_get(QueueType::GRAPHICS);
	CommandQueue present_queue = backend->queue_get(QueueType::PRESENT);

	Swapchain swapchain = backend->swapchain_create();
	backend->swapchain_resize(
			graphics_queue, swapchain, { WINDOW_WIDTH, WINDOW_HEIGHT }, true /* vsync */);

	// Create Command Pool and Buffer
	CommandPool cmd_pool = backend->command_pool_create(graphics_queue);
	CommandBuffer cmd = backend->command_pool_allocate(cmd_pool);

	// Synchronization Primitives
	Semaphore image_available_sem = backend->semaphore_create();
	Semaphore render_finished_sem = backend->semaphore_create();
	Fence frame_fence = backend->fence_create();

	bool quit = false;
	SDL_Event e;
	float time = 0.0f;

	while (!quit) {
		while (SDL_PollEvent(&e) != 0) {
			if (e.type == SDL_QUIT) {
				quit = true;
			}
			if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_RESIZED) {
				backend->device_wait();
				backend->swapchain_resize(graphics_queue, swapchain,
						{ (uint32_t)e.window.data1, (uint32_t)e.window.data2 }, true);
			}
		}

		// Wait for the previous frame to finish processing on the CPU side
		backend->fence_wait(frame_fence);
		backend->fence_reset(frame_fence);

		// Acquire the next image from the swapchain
		// This tells the GPU: "Give me an image index I can draw into."
		// It signals 'image_available_sem' when the image is actually ready to be written to.
		uint32_t image_index = 0;
		auto acquire_result =
				backend->swapchain_acquire_image(swapchain, image_available_sem, &image_index);

		if (!acquire_result) {
			// If acquire failed (e.g. window resized), handle it or skip frame
			continue;
		}
		Image swapchain_image = *acquire_result;

		// Record Commands
		backend->command_reset(cmd);
		backend->command_begin(cmd);

		// Transition Image Layout for Clearing
		// Images coming from the swapchain are usually in an UNDEFINED state.
		// command_clear_color requires the image to be in ImageLayout::GENERAL.
		backend->command_transition_image(
				cmd, swapchain_image, ImageLayout::UNDEFINED, ImageLayout::GENERAL);

		// Clear the Screen
		// Calculate a color based on time
		time += 0.01f;
		Color clear_color = { (float)std::abs(sin(time)), (float)std::abs(cos(time)), 0.2f, 1.0f };

		backend->command_clear_color(cmd, swapchain_image, clear_color);

		// Transition Image Layout for Presentation
		// The presentation engine requires the image to be in PRESENT_SRC layout.
		backend->command_transition_image(
				cmd, swapchain_image, ImageLayout::GENERAL, ImageLayout::PRESENT_SRC);

		backend->command_end(cmd);

		// Submit Command Buffer
		// We wait for 'image_available_sem' (image is ready to write)
		// We signal 'render_finished_sem' (rendering is done)
		// We signal 'frame_fence' so CPU knows when this batch is done
		backend->queue_submit(
				graphics_queue, cmd, frame_fence, image_available_sem, render_finished_sem);

		// Present the image to the screen
		// Waits for 'render_finished_sem'
		backend->queue_present(present_queue, swapchain, render_finished_sem);
	}

	// Wait for GPU to finish all operations before destroying resources
	backend->device_wait();

	backend->fence_free(frame_fence);
	backend->semaphore_free(image_available_sem);
	backend->semaphore_free(render_finished_sem);

	// Command buffer is freed when pool is freed, but usually explicit free is good practice
	backend->command_pool_free(cmd_pool);
	backend->swapchain_free(swapchain);

	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
