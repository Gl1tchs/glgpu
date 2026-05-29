#include <catch2/catch_test_macros.hpp>

#define GPUKIT_EXPOSE_VULKAN
#include <gpukit/vulkan.h>

#include "test_common.h"

using namespace gpukit;
using namespace gpukit::vk;

TEST_CASE("Vulkan Expose — Device Handles", "[vulkan_expose][device]") {
	gpukit::test::ensure_test_device();

	SECTION("Instance is non-null") {
		REQUIRE(vulkan_get_instance() != VK_NULL_HANDLE);
	}

	SECTION("Physical device is non-null") {
		REQUIRE(vulkan_get_physical_device() != VK_NULL_HANDLE);
	}

	SECTION("Logical device is non-null") {
		REQUIRE(vulkan_get_device() != VK_NULL_HANDLE);
	}

	SECTION("Physical device has readable properties") {
		VkPhysicalDevice pd = vulkan_get_physical_device();
		VkPhysicalDeviceProperties props = {};
		vkGetPhysicalDeviceProperties(pd, &props);
		REQUIRE(props.deviceName[0] != '\0');
		REQUIRE(props.apiVersion != 0);
	}
}

// =========================================================================
// Format Conversion
// =========================================================================

TEST_CASE("Vulkan Expose — Format Conversion", "[vulkan_expose][format]") {
	SECTION("vulkan_format / gpukit_format round-trips") {
		const DataFormat cases[] = {
			DataFormat::R8G8B8A8_UNORM,
			DataFormat::R32G32B32A32_SFLOAT,
			DataFormat::D32_SFLOAT,
			DataFormat::R8_UNORM,
		};

		for (DataFormat fmt : cases) {
			VkFormat vk_fmt = vulkan_format(fmt);
			DataFormat back = gpukit_format(vk_fmt);
			REQUIRE(back == fmt);
		}
	}
}

// =========================================================================
// Null Handle Error Handling
// =========================================================================

TEST_CASE("Vulkan Expose — Null Handle Errors", "[vulkan_expose][errors]") {
	SECTION("Image null → INVALID_HANDLE") {
		auto res = vulkan_get_image_info(GL_NULL_HANDLE);
		REQUIRE(res.is_error());
		REQUIRE(res.error() == Error::INVALID_HANDLE);
	}

	SECTION("Buffer null → INVALID_HANDLE") {
		auto res = vulkan_get_buffer_info(GL_NULL_HANDLE);
		REQUIRE(res.is_error());
		REQUIRE(res.error() == Error::INVALID_HANDLE);
	}

	SECTION("Pipeline null → INVALID_HANDLE") {
		auto res = vulkan_get_pipeline_info(GL_NULL_HANDLE);
		REQUIRE(res.is_error());
		REQUIRE(res.error() == Error::INVALID_HANDLE);
	}

	SECTION("Shader null → INVALID_HANDLE") {
		auto res = vulkan_get_shader_info(GL_NULL_HANDLE);
		REQUIRE(res.is_error());
		REQUIRE(res.error() == Error::INVALID_HANDLE);
	}

	SECTION("UniformSet null → INVALID_HANDLE") {
		auto res = vulkan_get_uniform_set_info(GL_NULL_HANDLE);
		REQUIRE(res.is_error());
		REQUIRE(res.error() == Error::INVALID_HANDLE);
	}

	SECTION("RenderPass null → INVALID_HANDLE") {
		auto res = vulkan_get_render_pass_info(GL_NULL_HANDLE);
		REQUIRE(res.is_error());
		REQUIRE(res.error() == Error::INVALID_HANDLE);
	}

	SECTION("Swapchain null → INVALID_HANDLE") {
		auto res = vulkan_get_swapchain_info(GL_NULL_HANDLE);
		REQUIRE(res.is_error());
		REQUIRE(res.error() == Error::INVALID_HANDLE);
	}

	SECTION("QueryPool null → INVALID_HANDLE") {
		auto res = vulkan_get_query_pool_info(GL_NULL_HANDLE);
		REQUIRE(res.is_error());
		REQUIRE(res.error() == Error::INVALID_HANDLE);
	}

	SECTION("CommandQueue null → INVALID_HANDLE") {
		auto res = vulkan_get_queue_info(GL_NULL_HANDLE);
		REQUIRE(res.is_error());
		REQUIRE(res.error() == Error::INVALID_HANDLE);
	}
}

// =========================================================================
// Buffer Info
// =========================================================================

TEST_CASE("Vulkan Expose — Buffer Info", "[vulkan_expose][buffer]") {
	gpukit::test::ensure_test_device();

	SECTION("GPU buffer — VkBuffer non-null, size matches") {
		constexpr uint64_t size = 512;
		Buffer buf = buffer_create(size, BUFFER_USAGE_STORAGE_BUFFER_BIT, MemoryAllocationType::GPU)
							 .value();

		auto info = vulkan_get_buffer_info(buf);
		REQUIRE(info.is_ok());
		REQUIRE(info.value().buffer != VK_NULL_HANDLE);
		REQUIRE(info.value().size == size);

		buffer_free(buf);
	}

	SECTION("CPU buffer — view is VK_NULL_HANDLE for non-texel buffer") {
		Buffer buf =
				buffer_create(256, BUFFER_USAGE_UNIFORM_BUFFER_BIT, MemoryAllocationType::CPU).value();

		auto info = vulkan_get_buffer_info(buf).value();
		REQUIRE(info.buffer != VK_NULL_HANDLE);
		REQUIRE(info.view == VK_NULL_HANDLE);

		buffer_free(buf);
	}
}

TEST_CASE("Vulkan Expose — Image Info", "[vulkan_expose][image]") {
	gpukit::test::ensure_test_device();

	SECTION("Fields match creation parameters") {
		ImageCreateInfo ci = {};
		ci.size = { 128, 64 };
		ci.format = DataFormat::R8G8B8A8_UNORM;
		ci.usage = IMAGE_USAGE_SAMPLED_BIT | IMAGE_USAGE_TRANSFER_DST_BIT;
		ci.mipmapped = false;
		ci.samples = 1;

		Image img = image_create(ci).value();

		auto info = vulkan_get_image_info(img).value();
		REQUIRE(info.image != VK_NULL_HANDLE);
		REQUIRE(info.image_view != VK_NULL_HANDLE);
		REQUIRE(info.format == vulkan_format(DataFormat::R8G8B8A8_UNORM));
		REQUIRE(info.extent.width == 128);
		REQUIRE(info.extent.height == 64);
		REQUIRE(info.mip_levels == 1);
		REQUIRE(info.array_layers == 1);

		image_free(img);
	}

	SECTION("Mipmapped image — mip_levels > 1") {
		ImageCreateInfo ci = {};
		ci.size = { 256, 256 };
		ci.format = DataFormat::R8G8B8A8_UNORM;
		ci.usage = IMAGE_USAGE_SAMPLED_BIT | IMAGE_USAGE_TRANSFER_DST_BIT |
				IMAGE_USAGE_TRANSFER_SRC_BIT;
		ci.mipmapped = true;
		ci.samples = 1;

		Image img = image_create(ci).value();
		auto info = vulkan_get_image_info(img).value();
		REQUIRE(info.mip_levels > 1);

		image_free(img);
	}
}

TEST_CASE("Vulkan Expose — Shader Info", "[vulkan_expose][shader]") {
	gpukit::test::ensure_test_device();

	SECTION("Compute shader — pipeline_layout non-null, one stage") {
		Shader shader = gpukit::test::load_compute_shader("test_compute.comp").value();

		auto info = vulkan_get_shader_info(shader).value();
		REQUIRE(info.pipeline_layout != VK_NULL_HANDLE);
		REQUIRE(info.stage_count == 1);
		REQUIRE(info.stage_create_infos != nullptr);
		REQUIRE(info.stage_create_infos[0].stage == VK_SHADER_STAGE_COMPUTE_BIT);

		shader_free(shader);
	}

	SECTION("Graphics shader — two stages (vert + frag)") {
		Shader shader = gpukit::test::load_shader("test_pass.vert", "test_pass.frag").value();

		auto info = vulkan_get_shader_info(shader).value();
		REQUIRE(info.pipeline_layout != VK_NULL_HANDLE);
		REQUIRE(info.stage_count == 2);

		shader_free(shader);
	}
}

TEST_CASE("Vulkan Expose — Pipeline Info", "[vulkan_expose][pipeline]") {
	gpukit::test::ensure_test_device();

	SECTION("Compute pipeline — VkPipeline non-null") {
		Shader shader = gpukit::test::load_compute_shader("test_compute.comp").value();
		Pipeline pipe = compute_pipeline_create(shader).value();

		auto info = vulkan_get_pipeline_info(pipe).value();
		REQUIRE(info.pipeline != VK_NULL_HANDLE);

		pipeline_free(pipe);
		shader_free(shader);
	}

	SECTION("Graphics pipeline — VkPipeline non-null") {
		Shader shader = gpukit::test::load_shader("test_pass.vert", "test_pass.frag").value();

		GraphicsPipelineCreateInfo pci = {};
		pci.shader = shader;
		pci.primitive = RenderPrimitive::TRIANGLE_LIST;
		pci.rasterization_state.cull_mode = PolygonCullMode::DISABLED;
		pci.multisample_state.sample_count = 1;
		pci.color_blend_state = PipelineColorBlendState::create_disabled(1);
		pci.rendering_info.color_attachments = { DataFormat::R8G8B8A8_UNORM };

		Pipeline pipe = graphics_pipeline_create(pci).value();

		auto info = vulkan_get_pipeline_info(pipe).value();
		REQUIRE(info.pipeline != VK_NULL_HANDLE);

		pipeline_free(pipe);
		shader_free(shader);
	}
}

TEST_CASE("Vulkan Expose — Uniform Set Info", "[vulkan_expose][uniform]") {
	gpukit::test::ensure_test_device();

	SECTION("VkDescriptorSet and pool non-null") {
		Shader shader = gpukit::test::load_compute_shader("test_compute.comp").value();
		Buffer buf =
				buffer_create(256, BUFFER_USAGE_STORAGE_BUFFER_BIT, MemoryAllocationType::GPU).value();

		ShaderUniform uniform = {};
		uniform.binding = 0;
		uniform.type = ShaderUniformType::STORAGE_BUFFER;
		uniform.data.push_back(buf);

		UniformSet set = uniform_set_create(uniform, shader, 0).value();

		auto info = vulkan_get_uniform_set_info(set).value();
		REQUIRE(info.descriptor_set != VK_NULL_HANDLE);
		REQUIRE(info.descriptor_pool != VK_NULL_HANDLE);
		REQUIRE(info.bindless == false);
		REQUIRE(info.set_index == 0);

		uniform_set_free(set);
		buffer_free(buf);
		shader_free(shader);
	}
}

TEST_CASE("Vulkan Expose — Render Pass Info", "[vulkan_expose][renderpass]") {
	gpukit::test::ensure_test_device();

	SECTION("VkRenderPass non-null") {
		RenderPassAttachment att = {};
		att.format = DataFormat::R8G8B8A8_UNORM;
		att.load_op = AttachmentLoadOp::CLEAR;
		att.store_op = AttachmentStoreOp::STORE;
		att.final_layout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
		att.sample_count = 1;

		SubpassInfo subpass = {};
		subpass.attachments = { { 0, SUBPASS_ATTACHMENT_COLOR } };

		RenderPass rp = render_pass_create(att, subpass).value();

		auto info = vulkan_get_render_pass_info(rp).value();
		REQUIRE(info.render_pass != VK_NULL_HANDLE);

		render_pass_destroy(rp);
	}
}

TEST_CASE("Vulkan Expose — Query Pool Info", "[vulkan_expose][querypool]") {
	gpukit::test::ensure_test_device();

	SECTION("VkQueryPool non-null, count matches") {
		constexpr uint32_t count = 8;
		QueryPool pool = query_pool_create(count).value();

		auto info = vulkan_get_query_pool_info(pool).value();
		REQUIRE(info.pool != VK_NULL_HANDLE);
		REQUIRE(info.count == count);

		query_pool_free(pool);
	}
}

TEST_CASE("Vulkan Expose — Trivial Handle Casts", "[vulkan_expose][handles]") {
	gpukit::test::ensure_test_device();

	SECTION("Queue info — VkQueue non-null, family index is valid") {
		CommandQueue queue = queue_get(QueueType::GRAPHICS).value();
		auto info = vulkan_get_queue_info(queue).value();
		REQUIRE(info.queue != VK_NULL_HANDLE);
		REQUIRE(info.queue_family != UINT32_MAX);
	}

	SECTION("vulkan_fence — non-null VkFence") {
		Fence fence = fence_create(false);
		REQUIRE(vulkan_fence(fence) != VK_NULL_HANDLE);
		fence_free(fence);
	}

	SECTION("vulkan_semaphore — non-null VkSemaphore") {
		Semaphore sem = semaphore_create();
		REQUIRE(vulkan_semaphore(sem) != VK_NULL_HANDLE);
		semaphore_free(sem);
	}

	SECTION("vulkan_command_pool / vulkan_command_buffer — non-null") {
		CommandQueue queue = queue_get(QueueType::GRAPHICS).value();
		CommandPool pool = command_pool_create(queue).value();
		CommandBuffer cmd = command_pool_allocate(pool).value();

		REQUIRE(vulkan_command_pool(pool) != VK_NULL_HANDLE);
		REQUIRE(vulkan_command_buffer(cmd) != VK_NULL_HANDLE);

		command_pool_free(pool);
	}

	SECTION("vulkan_sampler — non-null VkSampler") {
		SamplerCreateInfo sci = {};
		sci.min_filter = ImageFiltering::LINEAR;
		sci.mag_filter = ImageFiltering::LINEAR;
		sci.wrap_u = ImageWrappingMode::REPEAT;
		sci.wrap_v = ImageWrappingMode::REPEAT;
		sci.wrap_w = ImageWrappingMode::REPEAT;

		Sampler sampler = sampler_create(sci).value();
		REQUIRE(vulkan_sampler(sampler) != VK_NULL_HANDLE);
		sampler_free(sampler);
	}
}
