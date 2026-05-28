#include <catch2/catch_test_macros.hpp>

#include <cstring>

#include "test_common.h"

using namespace gpukit;

static Pipeline make_pass_pipeline(Shader shader, DataFormat color_fmt) {
	GraphicsPipelineCreateInfo pci = {};
	pci.shader = shader;
	pci.primitive = RenderPrimitive::TRIANGLE_LIST;
	pci.rasterization_state.cull_mode = PolygonCullMode::DISABLED;
	pci.multisample_state.sample_count = 1;
	pci.color_blend_state = PipelineColorBlendState::create_disabled(1);
	pci.rendering_info.color_attachments = { color_fmt };
	auto res = graphics_pipeline_create(pci);
	REQUIRE(res.is_ok());
	return res.value();
}

// =========================================================================
// Legacy Render Pass
// =========================================================================

TEST_CASE("Legacy Render Pass", "[renderpass][legacy]") {
	gpukit::test::ensure_test_device();

	SECTION("Create and destroy render pass") {
		RenderPassAttachment att = {};
		att.format = DataFormat::R8G8B8A8_UNORM;
		att.load_op = AttachmentLoadOp::CLEAR;
		att.store_op = AttachmentStoreOp::STORE;
		att.final_layout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
		att.sample_count = 1;
		att.is_depth_attachment = false;

		SubpassAttachment sub_att = { 0, SUBPASS_ATTACHMENT_COLOR };
		SubpassInfo subpass = { { sub_att } };

		auto rp_res = render_pass_create(att, subpass);
		REQUIRE(rp_res.is_ok());
		RenderPass rp = rp_res.value();
		REQUIRE(rp != GL_NULL_HANDLE);
		REQUIRE(render_pass_destroy(rp).is_ok());
	}

	SECTION("Create render pass with depth attachment") {
		std::vector<RenderPassAttachment> atts = {
			{ DataFormat::R8G8B8A8_UNORM,
					AttachmentLoadOp::CLEAR,
					AttachmentStoreOp::STORE,
					ImageLayout::COLOR_ATTACHMENT_OPTIMAL,
					1, false },
			{ DataFormat::D32_SFLOAT,
					AttachmentLoadOp::CLEAR,
					AttachmentStoreOp::DONT_CARE,
					ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
					1, true },
		};
		SubpassAttachment color_sub = { 0, SUBPASS_ATTACHMENT_COLOR };
		SubpassAttachment depth_sub = { 1, SUBPASS_ATTACHMENT_DEPTH_STENCIL };
		SubpassInfo subpass = { { color_sub, depth_sub } };

		auto rp_res = render_pass_create(atts, subpass);
		REQUIRE(rp_res.is_ok());
		REQUIRE(render_pass_destroy(rp_res.value()).is_ok());
	}

	SECTION("Create and destroy framebuffer") {
		RenderPassAttachment att = {};
		att.format = DataFormat::R8G8B8A8_UNORM;
		att.load_op = AttachmentLoadOp::CLEAR;
		att.store_op = AttachmentStoreOp::STORE;
		att.final_layout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
		att.sample_count = 1;

		SubpassAttachment sub_att = { 0, SUBPASS_ATTACHMENT_COLOR };
		SubpassInfo subpass = { { sub_att } };

		auto rp_res = render_pass_create(att, subpass);
		REQUIRE(rp_res.is_ok());
		RenderPass rp = rp_res.value();

		ImageCreateInfo img_info = {};
		img_info.size = { 128, 128 };
		img_info.format = DataFormat::R8G8B8A8_UNORM;
		img_info.usage = IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		img_info.samples = 1;

		auto img_res = image_create(img_info);
		REQUIRE(img_res.is_ok());
		Image img = img_res.value();

		auto fb_res = frame_buffer_create(rp, img, { 128, 128 });
		REQUIRE(fb_res.is_ok());
		FrameBuffer fb = fb_res.value();
		REQUIRE(fb != GL_NULL_HANDLE);

		REQUIRE(frame_buffer_destroy(fb).is_ok());
		REQUIRE(image_free(img).is_ok());
		REQUIRE(render_pass_destroy(rp).is_ok());
	}

	SECTION("Record and submit legacy render pass commands") {
		constexpr uint32_t W = 64, H = 64;

		RenderPassAttachment att = {};
		att.format = DataFormat::R8G8B8A8_UNORM;
		att.load_op = AttachmentLoadOp::CLEAR;
		att.store_op = AttachmentStoreOp::STORE;
		att.final_layout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
		att.sample_count = 1;

		SubpassAttachment sub_att = { 0, SUBPASS_ATTACHMENT_COLOR };
		SubpassInfo subpass = { { sub_att } };

		auto rp_res = render_pass_create(att, subpass);
		REQUIRE(rp_res.is_ok());
		RenderPass rp = rp_res.value();

		ImageCreateInfo img_info = {};
		img_info.size = { W, H };
		img_info.format = DataFormat::R8G8B8A8_UNORM;
		img_info.usage = IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		img_info.samples = 1;

		auto img_res = image_create(img_info);
		REQUIRE(img_res.is_ok());
		Image img = img_res.value();

		auto fb_res = frame_buffer_create(rp, img, { W, H });
		REQUIRE(fb_res.is_ok());
		FrameBuffer fb = fb_res.value();

		// Build a pipeline compatible with the legacy render pass
		auto shader_res = gpukit::test::load_shader("test_pass.vert", "test_pass.frag");
		REQUIRE(shader_res.is_ok());
		Shader shader = shader_res.value();

		GraphicsPipelineCreateInfo pci = {};
		pci.shader = shader;
		pci.primitive = RenderPrimitive::TRIANGLE_LIST;
		pci.rasterization_state.cull_mode = PolygonCullMode::DISABLED;
		pci.multisample_state.sample_count = 1;
		pci.color_blend_state = PipelineColorBlendState::create_disabled(1);
		pci.render_pass = rp;

		auto pipe_res = graphics_pipeline_create(pci);
		REQUIRE(pipe_res.is_ok());
		Pipeline pipeline = pipe_res.value();

		auto queue_res = queue_get(QueueType::GRAPHICS);
		REQUIRE(queue_res.is_ok());
		CommandQueue queue = queue_res.value();

		auto pool_res = command_pool_create(queue);
		REQUIRE(pool_res.is_ok());
		CommandPool pool = pool_res.value();

		auto cmd_res = command_pool_allocate(pool);
		REQUIRE(cmd_res.is_ok());
		CommandBuffer cmd = cmd_res.value();

		REQUIRE(command_begin(cmd).is_ok());
		REQUIRE(command_begin_render_pass(cmd, rp, fb, { W, H }, COLOR_BLACK).is_ok());
		REQUIRE(command_bind_graphics_pipeline(cmd, pipeline).is_ok());
		REQUIRE(command_set_viewport(cmd, { W, H }).is_ok());
		REQUIRE(command_set_scissor(cmd, { W, H }).is_ok());
		REQUIRE(command_draw(cmd, 3, 1).is_ok());
		REQUIRE(command_end_render_pass(cmd).is_ok());
		REQUIRE(command_end(cmd).is_ok());

		Fence fence = fence_create(false);
		REQUIRE(queue_submit(queue, cmd, fence).is_ok());
		REQUIRE(fence_wait(fence).is_ok());

		REQUIRE(fence_free(fence).is_ok());
		REQUIRE(command_pool_free(pool).is_ok());
		REQUIRE(pipeline_free(pipeline).is_ok());
		REQUIRE(shader_free(shader).is_ok());
		REQUIRE(frame_buffer_destroy(fb).is_ok());
		REQUIRE(image_free(img).is_ok());
		REQUIRE(render_pass_destroy(rp).is_ok());
	}

}

// =========================================================================
// Dynamic Rendering
// =========================================================================

TEST_CASE("Dynamic Rendering", "[renderpass][dynamic]") {
	gpukit::test::ensure_test_device();

	SECTION("Record and submit empty dynamic rendering pass") {
		ImageCreateInfo img_info = {};
		img_info.size = { 64, 64 };
		img_info.format = DataFormat::R8G8B8A8_UNORM;
		img_info.usage = IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		img_info.samples = 1;

		auto img_res = image_create(img_info);
		REQUIRE(img_res.is_ok());
		Image img = img_res.value();

		auto queue_res = queue_get(QueueType::GRAPHICS);
		REQUIRE(queue_res.is_ok());
		CommandQueue queue = queue_res.value();

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
		REQUIRE(command_transition_image(cmd, img, ImageLayout::UNDEFINED,
					ImageLayout::COLOR_ATTACHMENT_OPTIMAL).is_ok());
		REQUIRE(command_begin_rendering(cmd, { 64, 64 }, color_att).is_ok());
		REQUIRE(command_end_rendering(cmd).is_ok());
		REQUIRE(command_end(cmd).is_ok());

		Fence fence = fence_create(false);
		REQUIRE(queue_submit(queue, cmd, fence).is_ok());
		REQUIRE(fence_wait(fence).is_ok());

		REQUIRE(fence_free(fence).is_ok());
		REQUIRE(command_pool_free(pool).is_ok());
		REQUIRE(image_free(img).is_ok());
	}

	SECTION("Draw triangle, readback confirms non-black pixels") {
		constexpr uint32_t W = 32, H = 32;
		constexpr uint64_t PIXEL_BYTES = W * H * 4;

		auto shader_res = gpukit::test::load_shader("test_pass.vert", "test_pass.frag");
		REQUIRE(shader_res.is_ok());
		Shader shader = shader_res.value();
		Pipeline pipeline = make_pass_pipeline(shader, DataFormat::R8G8B8A8_UNORM);

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

		auto queue_res = queue_get(QueueType::GRAPHICS);
		REQUIRE(queue_res.is_ok());
		CommandQueue queue = queue_res.value();

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

		REQUIRE(command_transition_image(cmd, img, ImageLayout::UNDEFINED,
					ImageLayout::COLOR_ATTACHMENT_OPTIMAL).is_ok());

		REQUIRE(command_begin_rendering(cmd, { W, H }, color_att).is_ok());
		REQUIRE(command_bind_graphics_pipeline(cmd, pipeline).is_ok());
		REQUIRE(command_set_viewport(cmd, { W, H }).is_ok());
		REQUIRE(command_set_scissor(cmd, { W, H }).is_ok());
		REQUIRE(command_draw(cmd, 3, 1).is_ok());
		REQUIRE(command_end_rendering(cmd).is_ok());

		REQUIRE(command_transition_image(cmd, img, ImageLayout::COLOR_ATTACHMENT_OPTIMAL,
					ImageLayout::TRANSFER_SRC_OPTIMAL).is_ok());

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

		// The shader outputs solid red (1,0,0,1). At least the center pixel
		// should be non-black after drawing the triangle.
		bool any_red = false;
		for (uint64_t i = 0; i < PIXEL_BYTES; i += 4) {
			if (pixels[i] > 0) {
				any_red = true;
				break;
			}
		}
		REQUIRE(any_red);
		REQUIRE(buffer_unmap(readback).is_ok());

		REQUIRE(fence_free(fence).is_ok());
		REQUIRE(command_pool_free(pool).is_ok());
		REQUIRE(buffer_free(readback).is_ok());
		REQUIRE(image_free(img).is_ok());
		REQUIRE(pipeline_free(pipeline).is_ok());
		REQUIRE(shader_free(shader).is_ok());
	}

	SECTION("Dynamic rendering with depth attachment") {
		constexpr uint32_t W = 32, H = 32;

		auto shader_res = gpukit::test::load_shader("test_pass.vert", "test_pass.frag");
		REQUIRE(shader_res.is_ok());
		Shader shader = shader_res.value();

		GraphicsPipelineCreateInfo pci = {};
		pci.shader = shader;
		pci.primitive = RenderPrimitive::TRIANGLE_LIST;
		pci.rasterization_state.cull_mode = PolygonCullMode::DISABLED;
		pci.multisample_state.sample_count = 1;
		pci.depth_stencil_state.enable_depth_test = true;
		pci.depth_stencil_state.enable_depth_write = true;
		pci.depth_stencil_state.depth_compare_operator = CompareOperator::LESS_OR_EQUAL;
		pci.color_blend_state = PipelineColorBlendState::create_disabled(1);
		pci.rendering_info.color_attachments = { DataFormat::R8G8B8A8_UNORM };
		pci.rendering_info.depth_attachment = DataFormat::D32_SFLOAT;

		auto pipe_res = graphics_pipeline_create(pci);
		REQUIRE(pipe_res.is_ok());
		Pipeline pipeline = pipe_res.value();

		ImageCreateInfo color_info = {};
		color_info.size = { W, H };
		color_info.format = DataFormat::R8G8B8A8_UNORM;
		color_info.usage = IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
		color_info.samples = 1;

		ImageCreateInfo depth_info = {};
		depth_info.size = { W, H };
		depth_info.format = DataFormat::D32_SFLOAT;
		depth_info.usage = IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
		depth_info.samples = 1;

		auto color_res = image_create(color_info);
		REQUIRE(color_res.is_ok());
		Image color_img = color_res.value();

		auto depth_res = image_create(depth_info);
		REQUIRE(depth_res.is_ok());
		Image depth_img = depth_res.value();

		auto queue_res = queue_get(QueueType::GRAPHICS);
		REQUIRE(queue_res.is_ok());
		CommandQueue queue = queue_res.value();

		auto pool_res = command_pool_create(queue);
		REQUIRE(pool_res.is_ok());
		CommandPool pool = pool_res.value();

		auto cmd_res = command_pool_allocate(pool);
		REQUIRE(cmd_res.is_ok());
		CommandBuffer cmd = cmd_res.value();

		RenderingAttachment color_att = {};
		color_att.image = color_img;
		color_att.layout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
		color_att.load_op = AttachmentLoadOp::CLEAR;
		color_att.store_op = AttachmentStoreOp::STORE;
		color_att.clear_color = COLOR_BLACK;

		REQUIRE(command_begin(cmd).is_ok());
		REQUIRE(command_transition_image(cmd, color_img,
					ImageLayout::UNDEFINED, ImageLayout::COLOR_ATTACHMENT_OPTIMAL).is_ok());
		REQUIRE(command_transition_image(cmd, depth_img,
					ImageLayout::UNDEFINED, ImageLayout::DEPTH_STENCIL_ATTACHMENT_OPTIMAL).is_ok());
		REQUIRE(command_begin_rendering(cmd, { W, H }, color_att, depth_img).is_ok());
		REQUIRE(command_bind_graphics_pipeline(cmd, pipeline).is_ok());
		REQUIRE(command_set_viewport(cmd, { W, H }).is_ok());
		REQUIRE(command_set_scissor(cmd, { W, H }).is_ok());
		REQUIRE(command_draw(cmd, 3, 1).is_ok());
		REQUIRE(command_end_rendering(cmd).is_ok());
		REQUIRE(command_end(cmd).is_ok());

		Fence fence = fence_create(false);
		REQUIRE(queue_submit(queue, cmd, fence).is_ok());
		REQUIRE(fence_wait(fence).is_ok());

		REQUIRE(fence_free(fence).is_ok());
		REQUIRE(command_pool_free(pool).is_ok());
		REQUIRE(image_free(depth_img).is_ok());
		REQUIRE(image_free(color_img).is_ok());
		REQUIRE(pipeline_free(pipeline).is_ok());
		REQUIRE(shader_free(shader).is_ok());
	}
}

// =========================================================================
// Command operations
// =========================================================================

TEST_CASE("Command Operations", "[command]") {
	gpukit::test::ensure_test_device();

	SECTION("command_clear_color on a color image") {
		ImageCreateInfo img_info = {};
		img_info.size = { 32, 32 };
		img_info.format = DataFormat::R8G8B8A8_UNORM;
		img_info.usage = IMAGE_USAGE_COLOR_ATTACHMENT_BIT | IMAGE_USAGE_TRANSFER_DST_BIT;
		img_info.samples = 1;

		auto img_res = image_create(img_info);
		REQUIRE(img_res.is_ok());
		Image img = img_res.value();

		auto queue_res = queue_get(QueueType::GRAPHICS);
		REQUIRE(queue_res.is_ok());
		CommandQueue queue = queue_res.value();

		auto pool_res = command_pool_create(queue);
		REQUIRE(pool_res.is_ok());
		CommandPool pool = pool_res.value();

		auto cmd_res = command_pool_allocate(pool);
		REQUIRE(cmd_res.is_ok());
		CommandBuffer cmd = cmd_res.value();

		REQUIRE(command_begin(cmd).is_ok());
		REQUIRE(command_transition_image(cmd, img,
					ImageLayout::UNDEFINED, ImageLayout::GENERAL).is_ok());
		REQUIRE(command_clear_color(cmd, img, COLOR_BLACK).is_ok());
		REQUIRE(command_end(cmd).is_ok());

		Fence fence = fence_create(false);
		REQUIRE(queue_submit(queue, cmd, fence).is_ok());
		REQUIRE(fence_wait(fence).is_ok());

		REQUIRE(fence_free(fence).is_ok());
		REQUIRE(command_pool_free(pool).is_ok());
		REQUIRE(image_free(img).is_ok());
	}

	SECTION("command_generate_mipmaps") {
		ImageCreateInfo img_info = {};
		img_info.size = { 128, 128 };
		img_info.format = DataFormat::R8G8B8A8_UNORM;
		img_info.usage = IMAGE_USAGE_SAMPLED_BIT | IMAGE_USAGE_TRANSFER_DST_BIT | IMAGE_USAGE_TRANSFER_SRC_BIT;
		img_info.mipmapped = true;
		img_info.samples = 1;

		auto img_res = image_create(img_info);
		REQUIRE(img_res.is_ok());
		Image img = img_res.value();

		// Upload initial data so mip generation has something to work with
		constexpr size_t SIZE = 128 * 128 * 4;
		uint8_t pixels[SIZE];
		memset(pixels, 0x80, SIZE);
		REQUIRE(image_upload(img, pixels, SIZE).is_ok());

		auto queue_res = queue_get(QueueType::GRAPHICS);
		REQUIRE(queue_res.is_ok());
		CommandQueue queue = queue_res.value();

		auto pool_res = command_pool_create(queue);
		REQUIRE(pool_res.is_ok());
		CommandPool pool = pool_res.value();

		auto cmd_res = command_pool_allocate(pool);
		REQUIRE(cmd_res.is_ok());
		CommandBuffer cmd = cmd_res.value();

		REQUIRE(command_begin(cmd).is_ok());
		REQUIRE(command_generate_mipmaps(cmd, img).is_ok());
		REQUIRE(command_end(cmd).is_ok());

		Fence fence = fence_create(false);
		REQUIRE(queue_submit(queue, cmd, fence).is_ok());
		REQUIRE(fence_wait(fence).is_ok());

		REQUIRE(fence_free(fence).is_ok());
		REQUIRE(command_pool_free(pool).is_ok());
		REQUIRE(image_free(img).is_ok());
	}

	SECTION("command_blit_image downsample") {
		ImageCreateInfo info = {};
		info.size = { 64, 64 };
		info.format = DataFormat::R8G8B8A8_UNORM;
		info.usage = IMAGE_USAGE_TRANSFER_SRC_BIT | IMAGE_USAGE_TRANSFER_DST_BIT | IMAGE_USAGE_SAMPLED_BIT;
		info.samples = 1;

		auto src_res = image_create(info);
		REQUIRE(src_res.is_ok());
		Image src = src_res.value();

		info.size = { 32, 32 };
		auto dst_res = image_create(info);
		REQUIRE(dst_res.is_ok());
		Image dst = dst_res.value();

		auto queue_res = queue_get(QueueType::GRAPHICS);
		REQUIRE(queue_res.is_ok());
		CommandQueue queue = queue_res.value();

		auto pool_res = command_pool_create(queue);
		REQUIRE(pool_res.is_ok());
		CommandPool pool = pool_res.value();

		auto cmd_res = command_pool_allocate(pool);
		REQUIRE(cmd_res.is_ok());
		CommandBuffer cmd = cmd_res.value();

		REQUIRE(command_begin(cmd).is_ok());
		REQUIRE(command_transition_image(cmd, src,
					ImageLayout::UNDEFINED, ImageLayout::TRANSFER_SRC_OPTIMAL).is_ok());
		REQUIRE(command_transition_image(cmd, dst,
					ImageLayout::UNDEFINED, ImageLayout::TRANSFER_DST_OPTIMAL).is_ok());
		REQUIRE(command_blit_image(cmd, src, dst, { 64, 64 }, { 32, 32 },
					0, 0, ImageFiltering::LINEAR).is_ok());
		REQUIRE(command_end(cmd).is_ok());

		Fence fence = fence_create(false);
		REQUIRE(queue_submit(queue, cmd, fence).is_ok());
		REQUIRE(fence_wait(fence).is_ok());

		REQUIRE(fence_free(fence).is_ok());
		REQUIRE(command_pool_free(pool).is_ok());
		REQUIRE(image_free(dst).is_ok());
		REQUIRE(image_free(src).is_ok());
	}

	SECTION("command_copy_image_to_image") {
		ImageCreateInfo info = {};
		info.size = { 32, 32 };
		info.format = DataFormat::R8G8B8A8_UNORM;
		info.usage = IMAGE_USAGE_TRANSFER_SRC_BIT | IMAGE_USAGE_TRANSFER_DST_BIT | IMAGE_USAGE_SAMPLED_BIT;
		info.samples = 1;

		auto src_res = image_create(info);
		REQUIRE(src_res.is_ok());
		Image src = src_res.value();

		auto dst_res = image_create(info);
		REQUIRE(dst_res.is_ok());
		Image dst = dst_res.value();

		auto queue_res = queue_get(QueueType::GRAPHICS);
		REQUIRE(queue_res.is_ok());
		CommandQueue queue = queue_res.value();

		auto pool_res = command_pool_create(queue);
		REQUIRE(pool_res.is_ok());
		CommandPool pool = pool_res.value();

		auto cmd_res = command_pool_allocate(pool);
		REQUIRE(cmd_res.is_ok());
		CommandBuffer cmd = cmd_res.value();

		REQUIRE(command_begin(cmd).is_ok());
		REQUIRE(command_transition_image(cmd, src,
					ImageLayout::UNDEFINED, ImageLayout::TRANSFER_SRC_OPTIMAL).is_ok());
		REQUIRE(command_transition_image(cmd, dst,
					ImageLayout::UNDEFINED, ImageLayout::TRANSFER_DST_OPTIMAL).is_ok());
		REQUIRE(command_copy_image_to_image(cmd, src, dst, { 32, 32 }, { 32, 32 }).is_ok());
		REQUIRE(command_end(cmd).is_ok());

		Fence fence = fence_create(false);
		REQUIRE(queue_submit(queue, cmd, fence).is_ok());
		REQUIRE(fence_wait(fence).is_ok());

		REQUIRE(fence_free(fence).is_ok());
		REQUIRE(command_pool_free(pool).is_ok());
		REQUIRE(image_free(dst).is_ok());
		REQUIRE(image_free(src).is_ok());
	}
}
