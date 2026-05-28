#include <catch2/catch_test_macros.hpp>

#include <cstring>

#include "test_common.h"

using namespace gpukit;

TEST_CASE("Command Buffer Reset", "[command]") {
	gpukit::test::ensure_test_device();

	SECTION("record, reset, re-record, submit") {
		auto queue_res = queue_get(QueueType::GRAPHICS);
		REQUIRE(queue_res.is_ok());
		CommandQueue queue = queue_res.value();

		auto pool_res = command_pool_create(queue);
		REQUIRE(pool_res.is_ok());
		CommandPool pool = pool_res.value();

		auto cmd_res = command_pool_allocate(pool);
		REQUIRE(cmd_res.is_ok());
		CommandBuffer cmd = cmd_res.value();

		// First record
		REQUIRE(command_begin(cmd).is_ok());
		REQUIRE(command_end(cmd).is_ok());

		// Reset individual command buffer (not the whole pool)
		REQUIRE(command_reset(cmd).is_ok());

		// Re-record after reset and submit
		REQUIRE(command_begin(cmd).is_ok());
		REQUIRE(command_end(cmd).is_ok());

		Fence fence = fence_create(false);
		REQUIRE(queue_submit(queue, cmd, fence).is_ok());
		REQUIRE(fence_wait(fence).is_ok());

		REQUIRE(fence_free(fence).is_ok());
		REQUIRE(command_pool_free(pool).is_ok());
	}
}

TEST_CASE("Vertex and Index Buffer Binding", "[command]") {
	gpukit::test::ensure_test_device();

	auto shader_res = gpukit::test::load_shader("test_pass.vert", "test_pass.frag");
	REQUIRE(shader_res.is_ok());
	Shader shader = shader_res.value();

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

	SECTION("command_bind_vertex_buffers inside dynamic rendering") {
		constexpr uint32_t W = 16, H = 16;

		ImageCreateInfo img_info = {};
		img_info.size = { W, H };
		img_info.format = DataFormat::R8G8B8A8_UNORM;
		img_info.usage = IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		img_info.samples = 1;

		auto img_res = image_create(img_info);
		REQUIRE(img_res.is_ok());
		Image img = img_res.value();

		auto vbuf_res = buffer_create(sizeof(float) * 8,
				BUFFER_USAGE_VERTEX_BUFFER_BIT | BUFFER_USAGE_TRANSFER_DST_BIT,
				MemoryAllocationType::GPU);
		REQUIRE(vbuf_res.is_ok());
		Buffer vbuf = vbuf_res.value();

		auto pool_res = command_pool_create(queue);
		REQUIRE(pool_res.is_ok());
		CommandPool pool = pool_res.value();

		auto cmd_res = command_pool_allocate(pool);
		REQUIRE(cmd_res.is_ok());
		CommandBuffer cmd = cmd_res.value();

		RenderingAttachment color_att = {};
		color_att.image = img;
		color_att.layout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
		color_att.load_op = AttachmentLoadOp::CLEAR;
		color_att.store_op = AttachmentStoreOp::STORE;
		color_att.clear_color = COLOR_BLACK;

		std::vector<Buffer> vbufs = { vbuf };
		std::vector<uint64_t> offsets = { 0 };

		REQUIRE(command_begin(cmd).is_ok());
		REQUIRE(command_transition_image(cmd, img,
					ImageLayout::UNDEFINED, ImageLayout::COLOR_ATTACHMENT_OPTIMAL).is_ok());
		REQUIRE(command_begin_rendering(cmd, { W, H }, color_att).is_ok());
		REQUIRE(command_bind_graphics_pipeline(cmd, pipeline).is_ok());
		REQUIRE(command_set_viewport(cmd, { W, H }).is_ok());
		REQUIRE(command_set_scissor(cmd, { W, H }).is_ok());
		REQUIRE(command_bind_vertex_buffers(cmd, 0, vbufs, offsets).is_ok());
		REQUIRE(command_end_rendering(cmd).is_ok());
		REQUIRE(command_end(cmd).is_ok());

		Fence fence = fence_create(false);
		REQUIRE(queue_submit(queue, cmd, fence).is_ok());
		REQUIRE(fence_wait(fence).is_ok());

		REQUIRE(fence_free(fence).is_ok());
		REQUIRE(command_pool_free(pool).is_ok());
		REQUIRE(buffer_free(vbuf).is_ok());
		REQUIRE(image_free(img).is_ok());
	}

	SECTION("command_bind_index_buffer inside dynamic rendering") {
		constexpr uint32_t W = 16, H = 16;

		ImageCreateInfo img_info = {};
		img_info.size = { W, H };
		img_info.format = DataFormat::R8G8B8A8_UNORM;
		img_info.usage = IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		img_info.samples = 1;

		auto img_res = image_create(img_info);
		REQUIRE(img_res.is_ok());
		Image img = img_res.value();

		auto ibuf_res = buffer_create(sizeof(uint16_t) * 6,
				BUFFER_USAGE_INDEX_BUFFER_BIT | BUFFER_USAGE_TRANSFER_DST_BIT,
				MemoryAllocationType::GPU);
		REQUIRE(ibuf_res.is_ok());
		Buffer ibuf = ibuf_res.value();

		auto pool_res = command_pool_create(queue);
		REQUIRE(pool_res.is_ok());
		CommandPool pool = pool_res.value();

		auto cmd_res = command_pool_allocate(pool);
		REQUIRE(cmd_res.is_ok());
		CommandBuffer cmd = cmd_res.value();

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
		REQUIRE(command_bind_index_buffer(cmd, ibuf, 0, IndexType::UINT16).is_ok());
		REQUIRE(command_end_rendering(cmd).is_ok());
		REQUIRE(command_end(cmd).is_ok());

		Fence fence = fence_create(false);
		REQUIRE(queue_submit(queue, cmd, fence).is_ok());
		REQUIRE(fence_wait(fence).is_ok());

		REQUIRE(fence_free(fence).is_ok());
		REQUIRE(command_pool_free(pool).is_ok());
		REQUIRE(buffer_free(ibuf).is_ok());
		REQUIRE(image_free(img).is_ok());
	}

	REQUIRE(pipeline_free(pipeline).is_ok());
	REQUIRE(shader_free(shader).is_ok());
}

TEST_CASE("Command Operations Extended", "[command]") {
	gpukit::test::ensure_test_device();

	auto queue_res = queue_get(QueueType::GRAPHICS);
	REQUIRE(queue_res.is_ok());
	CommandQueue queue = queue_res.value();

	SECTION("command_clear_depth on a depth image") {
		ImageCreateInfo info = {};
		info.size = { 64, 64 };
		info.format = DataFormat::D32_SFLOAT;
		info.usage = IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | IMAGE_USAGE_TRANSFER_DST_BIT;
		info.samples = 1;

		auto img_res = image_create(info);
		REQUIRE(img_res.is_ok());
		Image img = img_res.value();

		auto pool_res = command_pool_create(queue);
		REQUIRE(pool_res.is_ok());
		CommandPool pool = pool_res.value();

		auto cmd_res = command_pool_allocate(pool);
		REQUIRE(cmd_res.is_ok());
		CommandBuffer cmd = cmd_res.value();

		REQUIRE(command_begin(cmd).is_ok());
		REQUIRE(command_transition_image(cmd, img,
					ImageLayout::UNDEFINED, ImageLayout::GENERAL).is_ok());
		REQUIRE(command_clear_depth(cmd, img, 1.0f, 0).is_ok());
		REQUIRE(command_end(cmd).is_ok());

		Fence fence = fence_create(false);
		REQUIRE(queue_submit(queue, cmd, fence).is_ok());
		REQUIRE(fence_wait(fence).is_ok());

		REQUIRE(fence_free(fence).is_ok());
		REQUIRE(command_pool_free(pool).is_ok());
		REQUIRE(image_free(img).is_ok());
	}

	SECTION("command_buffer_memory_barrier on a storage buffer") {
		auto buf_res = buffer_create(256, BUFFER_USAGE_STORAGE_BUFFER_BIT, MemoryAllocationType::GPU);
		REQUIRE(buf_res.is_ok());
		Buffer buf = buf_res.value();

		auto pool_res = command_pool_create(queue);
		REQUIRE(pool_res.is_ok());
		CommandPool pool = pool_res.value();

		auto cmd_res = command_pool_allocate(pool);
		REQUIRE(cmd_res.is_ok());
		CommandBuffer cmd = cmd_res.value();

		REQUIRE(command_begin(cmd).is_ok());
		REQUIRE(command_buffer_memory_barrier(cmd,
					BUFFER_USAGE_STORAGE_BUFFER_BIT,
					BUFFER_USAGE_VERTEX_BUFFER_BIT,
					buf).is_ok());
		REQUIRE(command_end(cmd).is_ok());

		Fence fence = fence_create(false);
		REQUIRE(queue_submit(queue, cmd, fence).is_ok());
		REQUIRE(fence_wait(fence).is_ok());

		REQUIRE(fence_free(fence).is_ok());
		REQUIRE(command_pool_free(pool).is_ok());
		REQUIRE(buffer_free(buf).is_ok());
	}

	SECTION("command_copy_buffer_to_image transfers pixel data") {
		constexpr uint32_t W = 8, H = 8;
		constexpr size_t PIXEL_BYTES = W * H * 4;
		uint8_t pixels[PIXEL_BYTES];
		memset(pixels, 0x7F, PIXEL_BYTES);

		auto src_res = buffer_create(PIXEL_BYTES, BUFFER_USAGE_TRANSFER_SRC_BIT, MemoryAllocationType::CPU);
		REQUIRE(src_res.is_ok());
		Buffer src = src_res.value();
		REQUIRE(buffer_upload(src, pixels, PIXEL_BYTES).is_ok());

		ImageCreateInfo img_info = {};
		img_info.size = { W, H };
		img_info.format = DataFormat::R8G8B8A8_UNORM;
		img_info.usage = IMAGE_USAGE_SAMPLED_BIT | IMAGE_USAGE_TRANSFER_DST_BIT;
		img_info.samples = 1;

		auto img_res = image_create(img_info);
		REQUIRE(img_res.is_ok());
		Image img = img_res.value();

		auto pool_res = command_pool_create(queue);
		REQUIRE(pool_res.is_ok());
		CommandPool pool = pool_res.value();

		auto cmd_res = command_pool_allocate(pool);
		REQUIRE(cmd_res.is_ok());
		CommandBuffer cmd = cmd_res.value();

		BufferImageCopyRegion region = {};
		region.image_subresource = { IMAGE_ASPECT_COLOR_BIT, 0, 0, 1 };
		region.image_extent = { W, H, 1 };

		REQUIRE(command_begin(cmd).is_ok());
		REQUIRE(command_transition_image(cmd, img,
					ImageLayout::UNDEFINED, ImageLayout::TRANSFER_DST_OPTIMAL).is_ok());
		REQUIRE(command_copy_buffer_to_image(cmd, src, img, region).is_ok());
		REQUIRE(command_end(cmd).is_ok());

		Fence fence = fence_create(false);
		REQUIRE(queue_submit(queue, cmd, fence).is_ok());
		REQUIRE(fence_wait(fence).is_ok());

		REQUIRE(fence_free(fence).is_ok());
		REQUIRE(command_pool_free(pool).is_ok());
		REQUIRE(image_free(img).is_ok());
		REQUIRE(buffer_free(src).is_ok());
	}

	SECTION("command_dispatch_indirect executes compute via indirect buffer") {
		auto shader_res = gpukit::test::load_compute_shader("test_compute.comp");
		REQUIRE(shader_res.is_ok());
		Shader shader = shader_res.value();

		auto pipe_res = compute_pipeline_create(shader);
		REQUIRE(pipe_res.is_ok());
		Pipeline pipeline = pipe_res.value();

		// Storage buffer: 64 floats all set to 1.0, will be multiplied by MULTIPLIER=2
		constexpr uint32_t COUNT = 64;
		float data[COUNT];
		for (uint32_t i = 0; i < COUNT; ++i)
			data[i] = 1.0f;

		auto sbuf_res = buffer_create(sizeof(data), BUFFER_USAGE_STORAGE_BUFFER_BIT, MemoryAllocationType::CPU);
		REQUIRE(sbuf_res.is_ok());
		Buffer sbuf = sbuf_res.value();
		REQUIRE(buffer_upload(sbuf, data, sizeof(data)).is_ok());

		auto set_res = uniform_set_create(shader, 0);
		REQUIRE(set_res.is_ok());
		UniformSet set = set_res.value();
		REQUIRE(uniform_set_update_buffer(set, 0, 0, sbuf).is_ok());

		// Indirect dispatch command: { x=1, y=1, z=1 } — 3 uint32_t values
		uint32_t indirect_cmd[3] = { 1, 1, 1 };
		auto ibuf_res = buffer_create(sizeof(indirect_cmd),
				BUFFER_USAGE_INDIRECT_BUFFER_BIT,
				MemoryAllocationType::CPU);
		REQUIRE(ibuf_res.is_ok());
		Buffer ibuf = ibuf_res.value();
		REQUIRE(buffer_upload(ibuf, indirect_cmd, sizeof(indirect_cmd)).is_ok());

		auto compute_queue_res = queue_get(QueueType::COMPUTE);
		REQUIRE(compute_queue_res.is_ok());
		CommandQueue cqueue = compute_queue_res.value();

		auto pool_res = command_pool_create(cqueue);
		REQUIRE(pool_res.is_ok());
		CommandPool pool = pool_res.value();

		auto cmd_res = command_pool_allocate(pool);
		REQUIRE(cmd_res.is_ok());
		CommandBuffer cmd = cmd_res.value();

		REQUIRE(command_begin(cmd).is_ok());
		REQUIRE(command_bind_compute_pipeline(cmd, pipeline).is_ok());
		REQUIRE(command_bind_uniform_sets(cmd, shader, 0, set, PipelineType::COMPUTE).is_ok());
		REQUIRE(command_dispatch_indirect(cmd, ibuf, 0).is_ok());
		REQUIRE(command_end(cmd).is_ok());

		Fence fence = fence_create(false);
		REQUIRE(queue_submit(cqueue, cmd, fence).is_ok());
		REQUIRE(fence_wait(fence).is_ok());

		auto map_res = buffer_map(sbuf);
		REQUIRE(map_res.is_ok());
		REQUIRE(buffer_invalidate(sbuf).is_ok());
		const float* result = reinterpret_cast<const float*>(map_res.value());
		for (uint32_t i = 0; i < COUNT; ++i)
			REQUIRE(result[i] == 2.0f); // MULTIPLIER default = 2
		REQUIRE(buffer_unmap(sbuf).is_ok());

		REQUIRE(fence_free(fence).is_ok());
		REQUIRE(command_pool_free(pool).is_ok());
		REQUIRE(uniform_set_free(set).is_ok());
		REQUIRE(buffer_free(ibuf).is_ok());
		REQUIRE(buffer_free(sbuf).is_ok());
		REQUIRE(pipeline_free(pipeline).is_ok());
		REQUIRE(shader_free(shader).is_ok());
	}

	SECTION("command_set_depth_bias with PIPELINE_DYNAMIC_STATE_DEPTH_BIAS") {
		auto shader_res = gpukit::test::load_shader("test_pass.vert", "test_pass.frag");
		REQUIRE(shader_res.is_ok());
		Shader shader = shader_res.value();

		GraphicsPipelineCreateInfo pci = {};
		pci.shader = shader;
		pci.primitive = RenderPrimitive::TRIANGLE_LIST;
		pci.rasterization_state.cull_mode = PolygonCullMode::DISABLED;
		pci.rasterization_state.depth_bias_enabled = true;
		pci.multisample_state.sample_count = 1;
		pci.color_blend_state = PipelineColorBlendState::create_disabled(1);
		pci.rendering_info.color_attachments = { DataFormat::R8G8B8A8_UNORM };
		pci.dynamic_state = PIPELINE_DYNAMIC_STATE_DEPTH_BIAS;

		auto pipe_res = graphics_pipeline_create(pci);
		REQUIRE(pipe_res.is_ok());
		Pipeline pipeline = pipe_res.value();

		constexpr uint32_t W = 16, H = 16;
		ImageCreateInfo img_info = {};
		img_info.size = { W, H };
		img_info.format = DataFormat::R8G8B8A8_UNORM;
		img_info.usage = IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		img_info.samples = 1;

		auto img_res = image_create(img_info);
		REQUIRE(img_res.is_ok());
		Image img = img_res.value();

		auto pool_res = command_pool_create(queue);
		REQUIRE(pool_res.is_ok());
		CommandPool pool = pool_res.value();

		auto cmd_res = command_pool_allocate(pool);
		REQUIRE(cmd_res.is_ok());
		CommandBuffer cmd = cmd_res.value();

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
		REQUIRE(command_set_depth_bias(cmd, 1.0f, 0.0f, 1.0f).is_ok());
		REQUIRE(command_draw(cmd, 3, 1).is_ok());
		REQUIRE(command_end_rendering(cmd).is_ok());
		REQUIRE(command_end(cmd).is_ok());

		Fence fence = fence_create(false);
		REQUIRE(queue_submit(queue, cmd, fence).is_ok());
		REQUIRE(fence_wait(fence).is_ok());

		REQUIRE(fence_free(fence).is_ok());
		REQUIRE(command_pool_free(pool).is_ok());
		REQUIRE(image_free(img).is_ok());
		REQUIRE(pipeline_free(pipeline).is_ok());
		REQUIRE(shader_free(shader).is_ok());
	}
}
