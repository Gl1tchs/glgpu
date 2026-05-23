#include <vector>

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

#include <glgpu/glgpu.h>
#include <glgpu_sdl2_glue.h>

constexpr int WINDOW_WIDTH = 800;
constexpr int WINDOW_HEIGHT = 600;

// Struct to hold per-frame resources for buffering
struct FrameData {
	gl::CommandPool cmd_pool;
	gl::CommandBuffer cmd;
	gl::Semaphore image_available_sem;
	gl::Semaphore render_finished_sem;
	gl::Fence frame_fence;

	void init(gl::Device* device, gl::CommandQueue graphics_queue) {
		// Create Command Pool and Buffer for this specific frame
		cmd_pool = device->command_pool_create(graphics_queue).value();
		cmd = device->command_pool_allocate(cmd_pool).value();

		// Synchronization Primitives for this frame
		image_available_sem = device->semaphore_create();
		render_finished_sem = device->semaphore_create();

		// Create fence Note: initial state of this fence
		// is signaled by default
		frame_fence = device->fence_create();
	}

	void destroy(gl::Device* device) {
		device->fence_free(frame_fence);
		device->semaphore_free(image_available_sem);
		device->semaphore_free(render_finished_sem);

		// Command buffer is freed when pool is freed
		device->command_pool_free(cmd_pool);
	}
};

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

	gl::DeviceCreateInfo info{
		.required_features = gl::DEVICE_FEATURE_SWAPCHAIN_BIT |
				gl::DEVICE_FEATURE_ENSURE_SURFACE_SUPPORT | gl::DEVICE_FEATURE_VALIDATION_LAYERS,
	};

	if (!gl::extract_sdl2_info(info, window)) {
		GL_ASSERT(false, "Only X11 and windows is supported.");
	}

	auto device = gl::Device::create(info).own();

	gl::CommandQueue graphics_queue = device->queue_get(gl::QueueType::GRAPHICS).value();
	gl::CommandQueue present_queue = device->queue_get(gl::QueueType::PRESENT).value();

	gl::Swapchain swapchain = device->swapchain_create().value();
	device->swapchain_resize(
			graphics_queue, swapchain, { WINDOW_WIDTH, WINDOW_HEIGHT }, true /* vsync */);

	// Setup buffering
	uint32_t image_count = device->swapchain_get_image_count(swapchain).value();
	std::vector<FrameData> frames(image_count);

	for (auto& frame : frames) {
		frame.init(device.get(), graphics_queue);
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

		gl::Image swapchain_image = *acquire_result;

		// Record Commands
		device->command_reset(frame.cmd);
		device->command_begin(frame.cmd);

		// Transition Image Layout for Clearing
		// Images coming from the swapchain are usually in an UNDEFINED state.
		// command_clear_color requires the image to be in ImageLayout::GENERAL.
		device->command_transition_image(
				frame.cmd, swapchain_image, gl::ImageLayout::UNDEFINED, gl::ImageLayout::GENERAL);

		device->command_begin_label(frame.cmd, "HELLO WORLD", gl::COLOR_RED);

		// Clear the Screen
		// Calculate a color based on time
		time += 0.01f;
		gl::Color clear_color = { (float)std::abs(sin(time)), (float)std::abs(cos(time)), 0.2f,
			1.0f };

		device->command_clear_color(frame.cmd, swapchain_image, clear_color);

		device->command_end_label(frame.cmd);

		// Transition gl::Image Layout for Presentation
		// The presentation engine requires the image to be in PRESENT_SRC layout.
		device->command_transition_image(
				frame.cmd, swapchain_image, gl::ImageLayout::GENERAL, gl::ImageLayout::PRESENT_SRC);

		device->command_end(frame.cmd);

		// Submit Command Buffer
		// We wait for 'image_available_sem' (image is ready to write)
		// We signal 'render_finished_sem' (rendering is done)
		// We signal 'frame_fence' so CPU knows when this batch is done
		device->queue_submit(graphics_queue, frame.cmd, frame.frame_fence,
				frame.image_available_sem, frame.render_finished_sem);

		// Present the image to the screen
		// Waits for 'render_finished_sem'
		device->queue_present(present_queue, swapchain, frame.render_finished_sem);

		// Advance to the next frame data for the next loop iteration
		current_frame_index = (current_frame_index + 1) % frames.size();
	}

	// Wait for GPU to finish all operations before destroying resources
	device->device_wait();

	// Cleanup all frame data
	for (auto& frame : frames) {
		frame.destroy(device.get());
	}

	device->swapchain_free(swapchain);

	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}
