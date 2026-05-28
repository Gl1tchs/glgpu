#include <catch2/catch_test_macros.hpp>

#include "test_common.h"

using namespace gpukit;

struct ComputePC {
	float multiplier;
	uint32_t buf_offset;
	uint32_t count;
};

TEST_CASE("Push Constants - Compute", "[push_constants][compute]") {
	gpukit::test::ensure_test_device();

	auto shader_res = gpukit::test::load_compute_shader("test_push_const.comp");
	REQUIRE(shader_res.is_ok());
	Shader shader = shader_res.value();

	auto pipe_res = compute_pipeline_create(shader);
	REQUIRE(pipe_res.is_ok());
	Pipeline pipeline = pipe_res.value();

	auto queue_res = queue_get(QueueType::COMPUTE);
	REQUIRE(queue_res.is_ok());
	CommandQueue queue = queue_res.value();

	SECTION("Basic: multiplier is applied to every element") {
		constexpr uint32_t COUNT = 64;
		constexpr uint64_t BUF_SIZE = COUNT * sizeof(float);
		float input[COUNT];
		for (uint32_t i = 0; i < COUNT; ++i)
			input[i] = 1.0f;

		auto buf_res = buffer_create(BUF_SIZE, BUFFER_USAGE_STORAGE_BUFFER_BIT, MemoryAllocationType::CPU);
		REQUIRE(buf_res.is_ok());
		Buffer buf = buf_res.value();
		REQUIRE(buffer_upload(buf, input, BUF_SIZE).is_ok());

		auto set_res = uniform_set_create(shader, 0);
		REQUIRE(set_res.is_ok());
		UniformSet set = set_res.value();
		REQUIRE(uniform_set_update_buffer(set, 0, 0, buf).is_ok());

		auto pool_res = command_pool_create(queue);
		REQUIRE(pool_res.is_ok());
		CommandPool pool = pool_res.value();

		auto cmd_res = command_pool_allocate(pool);
		REQUIRE(cmd_res.is_ok());
		CommandBuffer cmd = cmd_res.value();

		ComputePC pc = { .multiplier = 7.0f, .buf_offset = 0, .count = COUNT };

		REQUIRE(command_begin(cmd).is_ok());
		REQUIRE(command_bind_compute_pipeline(cmd, pipeline).is_ok());
		REQUIRE(command_bind_uniform_sets(cmd, shader, 0, set, PipelineType::COMPUTE).is_ok());
		REQUIRE(command_push_constants(cmd, shader, 0, sizeof(pc), &pc).is_ok());
		REQUIRE(command_dispatch(cmd, 1, 1, 1).is_ok());
		REQUIRE(command_end(cmd).is_ok());

		Fence fence = fence_create(false);
		REQUIRE(queue_submit(queue, cmd, fence).is_ok());
		REQUIRE(fence_wait(fence).is_ok());

		auto map_res = buffer_map(buf);
		REQUIRE(map_res.is_ok());
		REQUIRE(buffer_invalidate(buf).is_ok());
		const float* result = reinterpret_cast<const float*>(map_res.value());
		for (uint32_t i = 0; i < COUNT; ++i)
			REQUIRE(result[i] == 7.0f);
		REQUIRE(buffer_unmap(buf).is_ok());

		REQUIRE(fence_free(fence).is_ok());
		REQUIRE(command_pool_free(pool).is_ok());
		REQUIRE(uniform_set_free(set).is_ok());
		REQUIRE(buffer_free(buf).is_ok());
	}

	SECTION("Partial: count field limits the affected range") {
		constexpr uint32_t COUNT = 64;
		constexpr uint64_t BUF_SIZE = COUNT * sizeof(float);
		float input[COUNT];
		for (uint32_t i = 0; i < COUNT; ++i)
			input[i] = 1.0f;

		auto buf_res = buffer_create(BUF_SIZE, BUFFER_USAGE_STORAGE_BUFFER_BIT, MemoryAllocationType::CPU);
		REQUIRE(buf_res.is_ok());
		Buffer buf = buf_res.value();
		REQUIRE(buffer_upload(buf, input, BUF_SIZE).is_ok());

		auto set_res = uniform_set_create(shader, 0);
		REQUIRE(set_res.is_ok());
		UniformSet set = set_res.value();
		REQUIRE(uniform_set_update_buffer(set, 0, 0, buf).is_ok());

		auto pool_res = command_pool_create(queue);
		REQUIRE(pool_res.is_ok());
		CommandPool pool = pool_res.value();

		auto cmd_res = command_pool_allocate(pool);
		REQUIRE(cmd_res.is_ok());
		CommandBuffer cmd = cmd_res.value();

		// Only process the first 32 elements
		ComputePC pc = { .multiplier = 4.0f, .buf_offset = 0, .count = 32 };

		REQUIRE(command_begin(cmd).is_ok());
		REQUIRE(command_bind_compute_pipeline(cmd, pipeline).is_ok());
		REQUIRE(command_bind_uniform_sets(cmd, shader, 0, set, PipelineType::COMPUTE).is_ok());
		REQUIRE(command_push_constants(cmd, shader, 0, sizeof(pc), &pc).is_ok());
		REQUIRE(command_dispatch(cmd, 1, 1, 1).is_ok());
		REQUIRE(command_end(cmd).is_ok());

		Fence fence = fence_create(false);
		REQUIRE(queue_submit(queue, cmd, fence).is_ok());
		REQUIRE(fence_wait(fence).is_ok());

		auto map_res = buffer_map(buf);
		REQUIRE(map_res.is_ok());
		REQUIRE(buffer_invalidate(buf).is_ok());
		const float* result = reinterpret_cast<const float*>(map_res.value());
		for (uint32_t i = 0; i < 32; ++i)
			REQUIRE(result[i] == 4.0f);
		for (uint32_t i = 32; i < COUNT; ++i)
			REQUIRE(result[i] == 1.0f);
		REQUIRE(buffer_unmap(buf).is_ok());

		REQUIRE(fence_free(fence).is_ok());
		REQUIRE(command_pool_free(pool).is_ok());
		REQUIRE(uniform_set_free(set).is_ok());
		REQUIRE(buffer_free(buf).is_ok());
	}

	SECTION("Two dispatches with different push constants in the same command buffer") {
		// Buffer of 128 floats: first 64 multiplied by 2.0, last 64 by 5.0
		constexpr uint32_t HALF = 64;
		constexpr uint64_t BUF_SIZE = HALF * 2 * sizeof(float);
		float input[HALF * 2];
		for (uint32_t i = 0; i < HALF * 2; ++i)
			input[i] = 1.0f;

		auto buf_res = buffer_create(BUF_SIZE, BUFFER_USAGE_STORAGE_BUFFER_BIT, MemoryAllocationType::CPU);
		REQUIRE(buf_res.is_ok());
		Buffer buf = buf_res.value();
		REQUIRE(buffer_upload(buf, input, BUF_SIZE).is_ok());

		auto set_res = uniform_set_create(shader, 0);
		REQUIRE(set_res.is_ok());
		UniformSet set = set_res.value();
		REQUIRE(uniform_set_update_buffer(set, 0, 0, buf).is_ok());

		auto pool_res = command_pool_create(queue);
		REQUIRE(pool_res.is_ok());
		CommandPool pool = pool_res.value();

		auto cmd_res = command_pool_allocate(pool);
		REQUIRE(cmd_res.is_ok());
		CommandBuffer cmd = cmd_res.value();

		REQUIRE(command_begin(cmd).is_ok());
		REQUIRE(command_bind_compute_pipeline(cmd, pipeline).is_ok());
		REQUIRE(command_bind_uniform_sets(cmd, shader, 0, set, PipelineType::COMPUTE).is_ok());

		ComputePC pc_first = { .multiplier = 2.0f, .buf_offset = 0, .count = HALF };
		REQUIRE(command_push_constants(cmd, shader, 0, sizeof(pc_first), &pc_first).is_ok());
		REQUIRE(command_dispatch(cmd, 1, 1, 1).is_ok());

		ComputePC pc_second = { .multiplier = 5.0f, .buf_offset = HALF, .count = HALF };
		REQUIRE(command_push_constants(cmd, shader, 0, sizeof(pc_second), &pc_second).is_ok());
		REQUIRE(command_dispatch(cmd, 1, 1, 1).is_ok());

		REQUIRE(command_end(cmd).is_ok());

		Fence fence = fence_create(false);
		REQUIRE(queue_submit(queue, cmd, fence).is_ok());
		REQUIRE(fence_wait(fence).is_ok());

		auto map_res = buffer_map(buf);
		REQUIRE(map_res.is_ok());
		REQUIRE(buffer_invalidate(buf).is_ok());
		const float* result = reinterpret_cast<const float*>(map_res.value());
		for (uint32_t i = 0; i < HALF; ++i)
			REQUIRE(result[i] == 2.0f);
		for (uint32_t i = HALF; i < HALF * 2; ++i)
			REQUIRE(result[i] == 5.0f);
		REQUIRE(buffer_unmap(buf).is_ok());

		REQUIRE(fence_free(fence).is_ok());
		REQUIRE(command_pool_free(pool).is_ok());
		REQUIRE(uniform_set_free(set).is_ok());
		REQUIRE(buffer_free(buf).is_ok());
	}

	REQUIRE(pipeline_free(pipeline).is_ok());
	REQUIRE(shader_free(shader).is_ok());
}

// =========================================================================
// Graphics push constants
// =========================================================================

TEST_CASE("Push Constants - Graphics", "[push_constants][graphics]") {
	gpukit::test::ensure_test_device();

	// test_pass.vert (no inputs) + test_push_const.frag (push constant color)
	auto vert_frag_res = gpukit::test::load_shader("test_pass.vert", "test_push_const.frag");
	REQUIRE(vert_frag_res.is_ok());
	Shader shader = vert_frag_res.value();

	GraphicsPipelineCreateInfo pci = {};
	pci.shader = shader;
	pci.primitive = RenderPrimitive::TRIANGLE_LIST;
	pci.rasterization_state.cull_mode = PolygonCullMode::DISABLED;
	pci.multisample_state.sample_count = 1;
	pci.color_blend_state = PipelineColorBlendState::create_disabled(1);
	pci.rendering_info.color_attachments = { DataFormat::R8G8B8A8_UNORM };

	auto pipe_res = graphics_pipeline_create(pci);
	REQUIRE(pipe_res.is_ok());
	Pipeline pipeline = pipe_res.value();

	auto queue_res = queue_get(QueueType::GRAPHICS);
	REQUIRE(queue_res.is_ok());
	CommandQueue queue = queue_res.value();

	SECTION("Fragment color from push constant verified by pixel readback") {
		constexpr uint32_t W = 32, H = 32;
		constexpr uint64_t PIXEL_BYTES = W * H * 4;

		ImageCreateInfo img_info = {};
		img_info.size = { W, H };
		img_info.format = DataFormat::R8G8B8A8_UNORM;
		img_info.usage = IMAGE_USAGE_COLOR_ATTACHMENT_BIT | IMAGE_USAGE_TRANSFER_SRC_BIT;
		img_info.samples = 1;

		auto img_res = image_create(img_info);
		REQUIRE(img_res.is_ok());
		Image img = img_res.value();

		auto readback_res = buffer_create(PIXEL_BYTES, BUFFER_USAGE_TRANSFER_DST_BIT, MemoryAllocationType::CPU);
		REQUIRE(readback_res.is_ok());
		Buffer readback = readback_res.value();

		auto pool_res = command_pool_create(queue);
		REQUIRE(pool_res.is_ok());
		CommandPool pool = pool_res.value();

		auto cmd_res = command_pool_allocate(pool);
		REQUIRE(cmd_res.is_ok());
		CommandBuffer cmd = cmd_res.value();

		// Push constant: solid green (R=0, G=255, B=0, A=255)
		const float green[4] = { 0.0f, 1.0f, 0.0f, 1.0f };

		RenderingAttachment color_att = {};
		color_att.image = img;
		color_att.layout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
		color_att.load_op = AttachmentLoadOp::CLEAR;
		color_att.store_op = AttachmentStoreOp::STORE;
		color_att.clear_color = COLOR_BLACK;

		REQUIRE(command_begin(cmd).is_ok());
		REQUIRE(command_transition_image(cmd, img,
					ImageLayout::UNDEFINED, ImageLayout::COLOR_ATTACHMENT_OPTIMAL).is_ok());
		REQUIRE(command_begin_rendering(cmd, { W, H }, color_att).is_ok());
		REQUIRE(command_bind_graphics_pipeline(cmd, pipeline).is_ok());
		REQUIRE(command_set_viewport(cmd, { W, H }).is_ok());
		REQUIRE(command_set_scissor(cmd, { W, H }).is_ok());
		REQUIRE(command_push_constants(cmd, shader, 0, sizeof(green), green).is_ok());
		REQUIRE(command_draw(cmd, 3, 1).is_ok());
		REQUIRE(command_end_rendering(cmd).is_ok());
		REQUIRE(command_transition_image(cmd, img,
					ImageLayout::COLOR_ATTACHMENT_OPTIMAL, ImageLayout::TRANSFER_SRC_OPTIMAL).is_ok());

		BufferImageCopyRegion copy_region = {};
		copy_region.image_subresource = { IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		copy_region.image_extent = { W, H, 1 };
		REQUIRE(command_copy_image_to_buffer(cmd, img, readback, copy_region).is_ok());
		REQUIRE(command_end(cmd).is_ok());

		Fence fence = fence_create(false);
		REQUIRE(queue_submit(queue, cmd, fence).is_ok());
		REQUIRE(fence_wait(fence).is_ok());

		auto map_res = buffer_map(readback);
		REQUIRE(map_res.is_ok());
		REQUIRE(buffer_invalidate(readback).is_ok());
		const uint8_t* pixels = map_res.value();

		// Green triangle was drawn: at least one pixel must have G > 0, R == 0
		bool found_green = false;
		for (uint64_t i = 0; i < PIXEL_BYTES; i += 4) {
			if (pixels[i] == 0 && pixels[i + 1] > 0) {
				found_green = true;
				break;
			}
		}
		REQUIRE(found_green);
		REQUIRE(buffer_unmap(readback).is_ok());

		REQUIRE(fence_free(fence).is_ok());
		REQUIRE(command_pool_free(pool).is_ok());
		REQUIRE(buffer_free(readback).is_ok());
		REQUIRE(image_free(img).is_ok());
	}

	SECTION("Two draws with different push constant colors in one command buffer") {
		constexpr uint32_t W = 32, H = 32;
		constexpr uint64_t PIXEL_BYTES = W * H * 4;

		ImageCreateInfo img_info = {};
		img_info.size = { W, H };
		img_info.format = DataFormat::R8G8B8A8_UNORM;
		img_info.usage = IMAGE_USAGE_COLOR_ATTACHMENT_BIT | IMAGE_USAGE_TRANSFER_SRC_BIT;
		img_info.samples = 1;

		auto img_res = image_create(img_info);
		REQUIRE(img_res.is_ok());
		Image img = img_res.value();

		auto readback_res = buffer_create(PIXEL_BYTES, BUFFER_USAGE_TRANSFER_DST_BIT, MemoryAllocationType::CPU);
		REQUIRE(readback_res.is_ok());
		Buffer readback = readback_res.value();

		auto pool_res = command_pool_create(queue);
		REQUIRE(pool_res.is_ok());
		CommandPool pool = pool_res.value();

		auto cmd_res = command_pool_allocate(pool);
		REQUIRE(cmd_res.is_ok());
		CommandBuffer cmd = cmd_res.value();

		// First draw: red. Second draw: blue (overwrites the same triangle).
		// The last push constant wins, so pixels should be blue.
		const float red[4]  = { 1.0f, 0.0f, 0.0f, 1.0f };
		const float blue[4] = { 0.0f, 0.0f, 1.0f, 1.0f };

		RenderingAttachment color_att = {};
		color_att.image = img;
		color_att.layout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
		color_att.load_op = AttachmentLoadOp::CLEAR;
		color_att.store_op = AttachmentStoreOp::STORE;
		color_att.clear_color = COLOR_BLACK;

		REQUIRE(command_begin(cmd).is_ok());
		REQUIRE(command_transition_image(cmd, img,
					ImageLayout::UNDEFINED, ImageLayout::COLOR_ATTACHMENT_OPTIMAL).is_ok());
		REQUIRE(command_begin_rendering(cmd, { W, H }, color_att).is_ok());
		REQUIRE(command_bind_graphics_pipeline(cmd, pipeline).is_ok());
		REQUIRE(command_set_viewport(cmd, { W, H }).is_ok());
		REQUIRE(command_set_scissor(cmd, { W, H }).is_ok());

		REQUIRE(command_push_constants(cmd, shader, 0, sizeof(red), red).is_ok());
		REQUIRE(command_draw(cmd, 3, 1).is_ok());

		REQUIRE(command_push_constants(cmd, shader, 0, sizeof(blue), blue).is_ok());
		REQUIRE(command_draw(cmd, 3, 1).is_ok());

		REQUIRE(command_end_rendering(cmd).is_ok());
		REQUIRE(command_transition_image(cmd, img,
					ImageLayout::COLOR_ATTACHMENT_OPTIMAL, ImageLayout::TRANSFER_SRC_OPTIMAL).is_ok());

		BufferImageCopyRegion copy_region = {};
		copy_region.image_subresource = { IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		copy_region.image_extent = { W, H, 1 };
		REQUIRE(command_copy_image_to_buffer(cmd, img, readback, copy_region).is_ok());
		REQUIRE(command_end(cmd).is_ok());

		Fence fence = fence_create(false);
		REQUIRE(queue_submit(queue, cmd, fence).is_ok());
		REQUIRE(fence_wait(fence).is_ok());

		auto map_res = buffer_map(readback);
		REQUIRE(map_res.is_ok());
		REQUIRE(buffer_invalidate(readback).is_ok());
		const uint8_t* pixels = map_res.value();

		// Blue triangle was drawn last — at least one pixel must have B > 0, R == 0
		bool found_blue = false;
		for (uint64_t i = 0; i < PIXEL_BYTES; i += 4) {
			if (pixels[i] == 0 && pixels[i + 2] > 0) {
				found_blue = true;
				break;
			}
		}
		REQUIRE(found_blue);
		REQUIRE(buffer_unmap(readback).is_ok());

		REQUIRE(fence_free(fence).is_ok());
		REQUIRE(command_pool_free(pool).is_ok());
		REQUIRE(buffer_free(readback).is_ok());
		REQUIRE(image_free(img).is_ok());
	}

	REQUIRE(pipeline_free(pipeline).is_ok());
	REQUIRE(shader_free(shader).is_ok());
}
