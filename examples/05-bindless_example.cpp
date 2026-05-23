#include <cstring>
#include <fstream>
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
	float uv[2];
	int tex_index;
};

std::vector<uint32_t> load_spirv_file(const std::string& filename) {
	std::ifstream file(filename, std::ios::in | std::ios::binary | std::ios::ate);

	if (!file.is_open()) {
		GL_LOG_ERROR("Unable to open SPIRV file at path: '{}'.", filename);
		return {};
	}

	size_t file_size = static_cast<size_t>(file.tellg());

	if (file_size % sizeof(uint32_t) != 0) {
		GL_LOG_ERROR("SPIRV file size is not a multiple of 4 (corrupted?): '{}'.", filename);
		return {};
	}

	std::vector<uint32_t> buffer(file_size / sizeof(uint32_t));
	file.seekg(0);
	file.read(reinterpret_cast<char*>(buffer.data()), file_size);

	return buffer;
}

gl::Image create_checkered_texture(gl::Device* device, uint32_t r, uint32_t g, uint32_t b) {
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

	gl::ImageCreateInfo img_info = {};
	img_info.size = { width, height };
	img_info.format = gl::DataFormat::R8G8B8A8_UNORM;
	img_info.usage = gl::IMAGE_USAGE_SAMPLED_BIT | gl::IMAGE_USAGE_TRANSFER_DST_BIT;
	img_info.mipmapped = false;
	img_info.samples = 1;

	gl::Image img = device->image_create(img_info).value();

	// Create a staging buffer
	uint64_t buffer_size = width * height * 4;
	gl::Buffer staging = device->buffer_create(buffer_size, gl::BUFFER_USAGE_TRANSFER_SRC_BIT,
									   gl::MemoryAllocationType::CPU)
								 .value();

	void* mapped = device->buffer_map(staging).value();
	std::memcpy(mapped, pixels.data(), buffer_size);
	device->buffer_flush(staging);
	device->buffer_unmap(staging);

	// Copy from staging buffer to image
	device->command_immediate_submit(
			[&](gl::CommandBuffer cmd) {
				// Transition image to TRANSFER_DST_OPTIMAL
				device->command_transition_image(cmd, img, gl::ImageLayout::UNDEFINED,
						gl::ImageLayout::TRANSFER_DST_OPTIMAL);

				gl::BufferImageCopyRegion copy_region = {};
				copy_region.buffer_offset = 0;
				copy_region.buffer_row_length = width;
				copy_region.buffer_image_height = height;
				copy_region.image_subresource = { .aspect_mask = gl::IMAGE_ASPECT_COLOR_BIT,
					.mip_level = 0,
					.base_array_layer = 0,
					.layer_count = 1 };
				copy_region.image_offset = { 0, 0, 0 };
				copy_region.image_extent = { width, height, 1 };

				device->command_copy_buffer_to_image(cmd, staging, img, { &copy_region, 1 });

				// Transition image to SHADER_READ_ONLY_OPTIMAL
				device->command_transition_image(cmd, img, gl::ImageLayout::TRANSFER_DST_OPTIMAL,
						gl::ImageLayout::SHADER_READ_ONLY_OPTIMAL);
			},
			gl::QueueType::TRANSFER);

	device->buffer_free(staging);

	return img;
}

int main(void) {
	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		GL_LOG_ERROR("SDL could not initialize! SDL_Error: {}", SDL_GetError());
		return 1;
	}

	SDL_Window* window = SDL_CreateWindow("GLGPU Bindless Resource Example",
			SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, WINDOW_WIDTH, WINDOW_HEIGHT,
			SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);

	if (!window) {
		GL_LOG_ERROR("Window could not be created! SDL_Error: {}", SDL_GetError());
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
	std::vector<uint32_t> vert_code = load_spirv_file("examples/assets/bindless_vert.spv");
	std::vector<uint32_t> frag_code = load_spirv_file("examples/assets/bindless_frag.spv");

	if (vert_code.empty() || frag_code.empty()) {
		GL_LOG_FATAL("Could not load shaders. Did you compile the slang/glsl files?");
		return 1;
	}

	gl::SpirvEntry vert_entry;
	vert_entry.byte_code = vert_code;
	vert_entry.stage = gl::SHADER_STAGE_VERTEX_BIT;

	gl::SpirvEntry frag_entry;
	frag_entry.byte_code = frag_code;
	frag_entry.stage = gl::SHADER_STAGE_FRAGMENT_BIT;

	std::vector<gl::SpirvEntry> entries = { vert_entry, frag_entry };
	gl::Shader shader = device->shader_create_from_bytecode(entries).value();
	device->set_debug_name(gl::ObjectType::SHADER, shader, "Bindless Shader");

	// Pipeline creation
	gl::DataFormat swapchain_format = device->swapchain_get_format(swapchain).value();

	gl::GraphicsPipelineCreateInfo pipeline_info{ .shader = shader,
		.primitive = gl::RenderPrimitive::TRIANGLE_LIST,
		.vertex_input_state = { .stride = sizeof(Vertex) },
		.color_blend_state = gl::PipelineColorBlendState::create_disabled(1),
		.rendering_info = { .color_attachments = { swapchain_format },
				.depth_attachment = gl::DataFormat::UNDEFINED } };

	gl::Pipeline pipeline = device->graphics_pipeline_create(pipeline_info).value();

	// Create checkered textures of 4 different colors
	gl::Image texture0 = create_checkered_texture(device.get(), 255, 0, 0); // Red
	gl::Image texture1 = create_checkered_texture(device.get(), 0, 255, 0); // Green
	gl::Image texture2 = create_checkered_texture(device.get(), 0, 0, 255); // Blue
	gl::Image texture3 = create_checkered_texture(device.get(), 255, 255, 0); // Yellow

	// Create sampler
	gl::SamplerCreateInfo sampler_info{};
	sampler_info.min_filter = gl::ImageFiltering::LINEAR;
	sampler_info.mag_filter = gl::ImageFiltering::LINEAR;
	sampler_info.wrap_u = gl::ImageWrappingMode::CLAMP_TO_EDGE;
	sampler_info.wrap_v = gl::ImageWrappingMode::CLAMP_TO_EDGE;
	gl::Sampler sampler = device->sampler_create(sampler_info).value();

	// Create the bindless uniform set.
	// Binding is at set 0, binding 0. Max count is 1000.
	gl::UniformSet bindless_set = device->uniform_set_create_bindless(shader, 0, 0, 1000).value();

	// Update the bindless set
	device->uniform_set_update_texture(bindless_set, 0, 0, texture0, sampler);
	device->uniform_set_update_texture(bindless_set, 0, 1, texture1, sampler);
	device->uniform_set_update_texture(bindless_set, 0, 2, texture2, sampler);
	device->uniform_set_update_texture(bindless_set, 0, 3, texture3, sampler);

	// Quad vertices (4 quads, each selecting a different texture index)
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
			.clear_color = { 0.15f, 0.15f, 0.15f, 1.0f },
		};

		gl::Vec2u draw_extent = device->swapchain_get_extent(swapchain).value();
		device->command_begin_rendering(frame.cmd, draw_extent, { &color_attachment, 1 });

		device->command_set_viewport(frame.cmd, draw_extent);
		device->command_set_scissor(frame.cmd, draw_extent);

		device->command_bind_graphics_pipeline(frame.cmd, pipeline);

		std::vector<gl::Buffer> vertex_buffers = { vertex_buffer };
		std::vector<uint64_t> offsets = { 0 };
		device->command_bind_vertex_buffers(frame.cmd, 0, vertex_buffers, offsets);

		// Bind the bindless uniform set
		std::vector<gl::UniformSet> uniform_sets = { bindless_set };
		device->command_bind_uniform_sets(
				frame.cmd, shader, 0, uniform_sets, gl::PipelineType::GRAPHICS);

		device->command_draw(frame.cmd, 24);

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
	device->uniform_set_free(bindless_set);
	device->sampler_free(sampler);
	device->image_free(texture0);
	device->image_free(texture1);
	device->image_free(texture2);
	device->image_free(texture3);

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
