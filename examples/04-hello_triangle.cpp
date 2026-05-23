#include <cstring>
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
	gl::Fence frame_fence;

	void init(gl::Device* device, gl::CommandQueue graphics_queue) {
		cmd_pool = device->command_pool_create(graphics_queue).value();
		cmd = device->command_pool_allocate(cmd_pool).value();
		image_available_sem = device->semaphore_create();
		frame_fence = device->fence_create();
	}

	void destroy(gl::Device* device) {
		device->fence_free(frame_fence);
		device->semaphore_free(image_available_sem);
		device->command_pool_free(cmd_pool);
	}
};

struct Vertex {
	float pos[2];
	float col[3];
};

int main(void) {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		GL_LOG_ERROR("SDL could not initialize! SDL_Error: {}", SDL_GetError());
		return 1;
	}

	SDL_Window* window = SDL_CreateWindow("GLGPU Hello Triangle", SDL_WINDOWPOS_UNDEFINED,
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

	std::vector<gl::Semaphore> render_finished_sems(image_count);
	for (uint32_t i = 0; i < image_count; i++) {
		render_finished_sems[i] = device->semaphore_create();
	}

	// Load shaders
	// Load shaders using the overloaded file-based shader API
	gl::Shader shader =
			device->shader_create("examples/assets/triangle.vert", "examples/assets/triangle.frag")
					.value();
	device->set_debug_name(gl::ObjectType::SHADER, shader, "My Shader");

	// Pipeline creation
	gl::DataFormat swapchain_format = device->swapchain_get_format(swapchain).value();

	gl::GraphicsPipelineCreateInfo pipeline_info{
		.shader = shader,
		.primitive = gl::RenderPrimitive::TRIANGLE_LIST,
		.vertex_input_state = { .stride = sizeof(Vertex) },
		.color_blend_state = gl::PipelineColorBlendState::create_disabled(1),
		.rendering_info = { .color_attachments = { swapchain_format },
				.depth_attachment = gl::DataFormat::UNDEFINED },
	};

	gl::Pipeline pipeline = device->graphics_pipeline_create(pipeline_info).value();

	// Vertex buffer setup
	Vertex vertices[] = { { { 0.0f, -0.5f }, { 1.0f, 0.0f, 0.0f } },
		{ { 0.5f, 0.5f }, { 0.0f, 1.0f, 0.0f } }, { { -0.5f, 0.5f }, { 0.0f, 0.0f, 1.0f } } };

	gl::Buffer vertex_buffer =
			device->buffer_create(sizeof(vertices), gl::BUFFER_USAGE_VERTEX_BUFFER_BIT,
						  gl::MemoryAllocationType::CPU)
					.value();

	void* raw_data = device->buffer_map(vertex_buffer).value();
	if (raw_data) {
		std::memcpy(raw_data, vertices, sizeof(vertices));
		device->buffer_unmap(vertex_buffer);
	} else {
		GL_LOG_FATAL("Failed to map vertex buffer!");
		return 1;
	}

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

		FrameData& frame = frames[current_frame_index];

		device->fence_wait(frame.frame_fence);
		device->fence_reset(frame.frame_fence);

		uint32_t image_index = 0;
		auto acquire_result =
				device->swapchain_acquire_image(swapchain, frame.image_available_sem, &image_index);

		if (!acquire_result)
			continue;

		gl::Image swapchain_image = *acquire_result;

		device->command_reset(frame.cmd);
		device->command_begin(frame.cmd);

		// Transition layout for rendering
		device->command_transition_image(frame.cmd, swapchain_image, gl::ImageLayout::UNDEFINED,
				gl::ImageLayout::COLOR_ATTACHMENT_OPTIMAL);

		gl::RenderingAttachment color_attachment{
			.image = swapchain_image,
			.layout = gl::ImageLayout::COLOR_ATTACHMENT_OPTIMAL,
			.load_op = gl::AttachmentLoadOp::CLEAR,
			.store_op = gl::AttachmentStoreOp::STORE,
			.clear_color = { 0.1f, 0.1f, 0.1f, 1.0f },
		};

		gl::Vec2u draw_extent = device->swapchain_get_extent(swapchain).value();
		device->command_begin_rendering(frame.cmd, draw_extent, { &color_attachment, 1 });

		device->command_set_viewport(frame.cmd, draw_extent);
		device->command_set_scissor(frame.cmd, draw_extent);

		device->command_bind_graphics_pipeline(frame.cmd, pipeline);

		std::vector<gl::Buffer> vertex_buffers = { vertex_buffer };
		std::vector<uint64_t> offsets = { 0 };
		device->command_bind_vertex_buffers(frame.cmd, 0, vertex_buffers, offsets);

		device->command_begin_label(frame.cmd, "My Debug Label", gl::Color{ 1.0, 0.0, 1.0, 1.0 });
		device->command_draw(frame.cmd, 3);
		device->command_end_label(frame.cmd);

		device->command_end_rendering(frame.cmd);

		// Transition layout for presentation
		device->command_transition_image(frame.cmd, swapchain_image,
				gl::ImageLayout::COLOR_ATTACHMENT_OPTIMAL, gl::ImageLayout::PRESENT_SRC);

		device->command_end(frame.cmd);

		device->queue_submit(graphics_queue, frame.cmd, frame.frame_fence,
				frame.image_available_sem, render_finished_sems[image_index]);

		device->queue_present(present_queue, swapchain, render_finished_sems[image_index]);

		current_frame_index = (current_frame_index + 1) % frames.size();
	}

	device->device_wait();

	// Cleanup
	device->buffer_free(vertex_buffer);
	device->pipeline_free(pipeline);
	device->shader_free(shader);

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
