#include <vector>

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

#include <gpukit/gpukit.h>
#include <gpukit_sdl2_glue.h>

constexpr int WINDOW_WIDTH = 800;
constexpr int WINDOW_HEIGHT = 600;

// Struct to hold per-frame resources for buffering
struct FrameData {
	gpukit::CommandPool cmd_pool;
	gpukit::CommandBuffer cmd;
	gpukit::Semaphore image_available_sem;
	gpukit::Fence frame_fence;

	void init(gpukit::Device* device, gpukit::CommandQueue graphics_queue) {
		// Create Command Pool and Buffer for this specific frame
		cmd_pool = device->command_pool_create(graphics_queue).value();
		cmd = device->command_pool_allocate(cmd_pool).value();

		// Synchronization Primitives for this frame
		image_available_sem = device->semaphore_create();

		// Create fence Note: initial state of this fence
		// is signaled by default
		frame_fence = device->fence_create();
	}

	void destroy(gpukit::Device* device) {
		device->fence_free(frame_fence);
		device->semaphore_free(image_available_sem);

		// Command buffer is freed when pool is freed
		device->command_pool_free(cmd_pool);
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

	auto device = gpukit::Device::create(info).own();

	gpukit::CommandQueue graphics_queue = device->queue_get(gpukit::QueueType::GRAPHICS).value();
	gpukit::CommandQueue present_queue = device->queue_get(gpukit::QueueType::PRESENT).value();

	gpukit::Swapchain swapchain = device->swapchain_create().value();
	device->swapchain_resize(
			graphics_queue, swapchain, { WINDOW_WIDTH, WINDOW_HEIGHT }, true /* vsync */);

	// Setup buffering
	uint32_t image_count = device->swapchain_get_image_count(swapchain).value();
	std::vector<FrameData> frames(image_count);

	for (auto& frame : frames) {
		frame.init(device.get(), graphics_queue);
	}

	std::vector<gpukit::Semaphore> render_finished_sems(image_count);
	for (uint32_t i = 0; i < image_count; i++) {
		render_finished_sems[i] = device->semaphore_create();
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

				image_count = device->swapchain_get_image_count(swapchain).value();
				frames.resize(image_count);
				for (auto& frame : frames) {
					frame.init(device.get(), graphics_queue);
				}

				render_finished_sems.resize(image_count);
				for (uint32_t i = 0; i < image_count; i++) {
					render_finished_sems[i] = device->semaphore_create();
				}

				current_frame_index = 0;
			}
		}

		// It should write into the next image every time
		// Get the data for the current frame in the buffer sequence
		FrameData& frame = frames[current_frame_index];

		// Wait for the previous frame to finish processing on the CPU side
		device->fence_wait(frame.frame_fence);
		device->fence_reset(frame.frame_fence);

		// Acquire the next image from the swapchain
		// This tells the GPU: "Give me an image index I can draw into."
		// It signals 'image_available_sem' when the image is actually ready to be written to.
		uint32_t image_index = 0;
		auto acquire_result =
				device->swapchain_acquire_image(swapchain, frame.image_available_sem, &image_index);

		// If acquire failed (e.g. window resized), handle it or skip frame
		if (!acquire_result)
			continue;

		gpukit::Image swapchain_image = *acquire_result;

		// Record Commands
		device->command_reset(frame.cmd);
		device->command_begin(frame.cmd);

		// Transition Image Layout for Clearing
		// Images coming from the swapchain are usually in an UNDEFINED state.
		// command_clear_color requires the image to be in ImageLayout::GENERAL.
		device->command_transition_image(
				frame.cmd, swapchain_image, gpukit::ImageLayout::UNDEFINED, gpukit::ImageLayout::GENERAL);

		device->command_begin_label(frame.cmd, "HELLO WORLD", gpukit::COLOR_RED);

		// Clear the Screen
		// Calculate a color based on time
		time += 0.01f;
		gpukit::Color clear_color = { (float)std::abs(sin(time)), (float)std::abs(cos(time)), 0.2f,
			1.0f };

		device->command_clear_color(frame.cmd, swapchain_image, clear_color);

		device->command_end_label(frame.cmd);

		// Transition gpukit::Image Layout for Presentation
		// The presentation engine requires the image to be in PRESENT_SRC layout.
		device->command_transition_image(
				frame.cmd, swapchain_image, gpukit::ImageLayout::GENERAL, gpukit::ImageLayout::PRESENT_SRC);

		device->command_end(frame.cmd);

		// Submit Command Buffer
		// We wait for 'image_available_sem' (image is ready to write)
		// We signal 'render_finished_sem' (rendering is done)
		// We signal 'frame_fence' so CPU knows when this batch is done
		device->queue_submit(graphics_queue, frame.cmd, frame.frame_fence,
				frame.image_available_sem, render_finished_sems[image_index]);

		// Present the image to the screen
		// Waits for 'render_finished_sem'
		device->queue_present(present_queue, swapchain, render_finished_sems[image_index]);

		// Advance to the next frame data for the next loop iteration
		current_frame_index = (current_frame_index + 1) % frames.size();
	}

	// Wait for GPU to finish all operations before destroying resources
	device->device_wait();

	// Cleanup all frame data
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
