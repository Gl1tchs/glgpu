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

struct Vertex {
	float pos[2];
	float uv[2];
	int tex_index;
};

gpukit::Image create_checkered_texture(uint32_t r, uint32_t g, uint32_t b) {
	const uint32_t width = 16;
	const uint32_t height = 16;
	std::vector<uint32_t> pixels(width * height);
	for (uint32_t y = 0; y < height; y++) {
		for (uint32_t x = 0; x < width; x++) {
			bool check = ((x / 2) + (y / 2)) % 2 == 0;
			uint32_t color = check ? 0xFFFFFFFF : (0xFF000000 | (b << 16) | (g << 8) | r);
			pixels[y * width + x] = color;
		}
	}

	gpukit::ImageCreateInfo img_info = {};
	img_info.size = { width, height };
	img_info.format = gpukit::DataFormat::R8G8B8A8_UNORM;
	img_info.usage = gpukit::IMAGE_USAGE_SAMPLED_BIT | gpukit::IMAGE_USAGE_TRANSFER_DST_BIT;
	img_info.mipmapped = false;
	img_info.samples = 1;

	gpukit::Image img = gpukit::image_create(img_info).value();

	gpukit::image_upload(img, pixels.data(), pixels.size() * sizeof(uint32_t));

	return img;
}

int main(void) {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		GPUKIT_LOG_ERROR("SDL could not initialize! SDL_Error: {}", SDL_GetError());
		return 1;
	}

	SDL_Window* window = SDL_CreateWindow("GPUKit Bindless Resource Example",
			SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH, WINDOW_HEIGHT,
			SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

	if (!window) {
		GPUKIT_LOG_ERROR("Window could not be created! SDL_Error: {}", SDL_GetError());
		return 1;
	}

	gpukit::DeviceCreateInfo info{
		.required_features = gpukit::DEVICE_FEATURE_SWAPCHAIN_BIT |
				gpukit::DEVICE_FEATURE_ENSURE_SURFACE_SUPPORT |
				gpukit::DEVICE_FEATURE_VALIDATION_LAYERS,
	};

	if (!gpukit::extract_sdl2_info(info, window)) {
		GPUKIT_ASSERT(false, "Only X11 and windows is supported.");
	}

	gpukit::init(info);

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

	gpukit::Shader shader =
			gpukit::shader_create("examples/assets/bindless.vert", "examples/assets/bindless.frag")
					.value();
	gpukit::set_debug_name(gpukit::ObjectType::SHADER, shader, "Bindless Shader");

	gpukit::DataFormat swapchain_format = gpukit::swapchain_get_format(swapchain).value();

	gpukit::GraphicsPipelineCreateInfo pipeline_info{ .shader = shader,
		.primitive = gpukit::RenderPrimitive::TRIANGLE_LIST,
		.vertex_input_state = { .stride = sizeof(Vertex) },
		.color_blend_state = gpukit::PipelineColorBlendState::create_disabled(1),
		.rendering_info = { .color_attachments = { swapchain_format },
				.depth_attachment = gpukit::DataFormat::UNDEFINED } };

	gpukit::Pipeline pipeline = gpukit::graphics_pipeline_create(pipeline_info).value();

	gpukit::Image texture0 = create_checkered_texture(255, 0, 0);
	gpukit::Image texture1 = create_checkered_texture(0, 255, 0);
	gpukit::Image texture2 = create_checkered_texture(0, 0, 255);
	gpukit::Image texture3 = create_checkered_texture(255, 255, 0);

	gpukit::SamplerCreateInfo sampler_info{};
	sampler_info.min_filter = gpukit::ImageFiltering::NEAREST;
	sampler_info.mag_filter = gpukit::ImageFiltering::NEAREST;
	sampler_info.wrap_u = gpukit::ImageWrappingMode::CLAMP_TO_EDGE;
	sampler_info.wrap_v = gpukit::ImageWrappingMode::CLAMP_TO_EDGE;
	gpukit::Sampler sampler = gpukit::sampler_create(sampler_info).value();

	gpukit::UniformSet bindless_set =
			gpukit::uniform_set_create_bindless(shader, 0, 0, 1000).value();

	gpukit::uniform_set_update_texture(bindless_set, 0, 0, texture0, sampler);
	gpukit::uniform_set_update_texture(bindless_set, 0, 1, texture1, sampler);
	gpukit::uniform_set_update_texture(bindless_set, 0, 2, texture2, sampler);
	gpukit::uniform_set_update_texture(bindless_set, 0, 3, texture3, sampler);

	Vertex vertices[] = {
		// Quad 0 (top-left, Red, index 0)
		{ { -0.75f, 0.75f }, { 0.0f, 0.0f }, 0 },
		{ { -0.1f, 0.75f }, { 1.0f, 0.0f }, 0 },
		{ { -0.1f, 0.1f }, { 1.0f, 1.0f }, 0 },
		{ { -0.75f, 0.75f }, { 0.0f, 0.0f }, 0 },
		{ { -0.1f, 0.1f }, { 1.0f, 1.0f }, 0 },
		{ { -0.75f, 0.1f }, { 0.0f, 1.0f }, 0 },

		// Quad 1 (top-right, Green, index 1)
		{ { 0.1f, 0.75f }, { 0.0f, 0.0f }, 1 },
		{ { 0.75f, 0.75f }, { 1.0f, 0.0f }, 1 },
		{ { 0.75f, 0.1f }, { 1.0f, 1.0f }, 1 },
		{ { 0.1f, 0.75f }, { 0.0f, 0.0f }, 1 },
		{ { 0.75f, 0.1f }, { 1.0f, 1.0f }, 1 },
		{ { 0.1f, 0.1f }, { 0.0f, 1.0f }, 1 },

		// Quad 2 (bottom-left, Blue, index 2)
		{ { -0.75f, -0.1f }, { 0.0f, 0.0f }, 2 },
		{ { -0.1f, -0.1f }, { 1.0f, 0.0f }, 2 },
		{ { -0.1f, -0.75f }, { 1.0f, 1.0f }, 2 },
		{ { -0.75f, -0.1f }, { 0.0f, 0.0f }, 2 },
		{ { -0.1f, -0.75f }, { 1.0f, 1.0f }, 2 },
		{ { -0.75f, -0.75f }, { 0.0f, 1.0f }, 2 },

		// Quad 3 (bottom-right, Yellow, index 3)
		{ { 0.1f, -0.1f }, { 0.0f, 0.0f }, 3 },
		{ { 0.75f, -0.1f }, { 1.0f, 0.0f }, 3 },
		{ { 0.75f, -0.75f }, { 1.0f, 1.0f }, 3 },
		{ { 0.1f, -0.1f }, { 0.0f, 0.0f }, 3 },
		{ { 0.75f, -0.75f }, { 1.0f, 1.0f }, 3 },
		{ { 0.1f, -0.75f }, { 0.0f, 1.0f }, 3 },
	};

	gpukit::Buffer vertex_buffer = gpukit::buffer_create(sizeof(vertices),
			gpukit::BUFFER_USAGE_VERTEX_BUFFER_BIT, gpukit::MemoryAllocationType::CPU)
										   .value();

	gpukit::buffer_upload(vertex_buffer, vertices, sizeof(vertices));

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

		gpukit::command_transition_image(frame.cmd, swapchain_image, gpukit::ImageLayout::UNDEFINED,
				gpukit::ImageLayout::COLOR_ATTACHMENT_OPTIMAL);

		gpukit::RenderingAttachment color_attachment{
			.image = swapchain_image,
			.layout = gpukit::ImageLayout::COLOR_ATTACHMENT_OPTIMAL,
			.load_op = gpukit::AttachmentLoadOp::CLEAR,
			.store_op = gpukit::AttachmentStoreOp::STORE,
			.clear_color = { 0.15f, 0.15f, 0.15f, 1.0f },
		};

		gpukit::Vec2u draw_extent = gpukit::swapchain_get_extent(swapchain).value();
		gpukit::command_begin_rendering(frame.cmd, draw_extent, { &color_attachment, 1 });

		gpukit::command_set_viewport(frame.cmd, draw_extent);
		gpukit::command_set_scissor(frame.cmd, draw_extent);

		gpukit::command_bind_graphics_pipeline(frame.cmd, pipeline);

		std::vector<gpukit::Buffer> vertex_buffers = { vertex_buffer };
		std::vector<uint64_t> offsets = { 0 };
		gpukit::command_bind_vertex_buffers(frame.cmd, 0, vertex_buffers, offsets);

		std::vector<gpukit::UniformSet> uniform_sets = { bindless_set };
		gpukit::command_bind_uniform_sets(
				frame.cmd, shader, 0, uniform_sets, gpukit::PipelineType::GRAPHICS);

		gpukit::command_draw(frame.cmd, 24);

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

	gpukit::buffer_free(vertex_buffer);
	gpukit::uniform_set_free(bindless_set);
	gpukit::sampler_free(sampler);
	gpukit::image_free(texture0);
	gpukit::image_free(texture1);
	gpukit::image_free(texture2);
	gpukit::image_free(texture3);
	gpukit::pipeline_free(pipeline);
	gpukit::shader_free(shader);

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
