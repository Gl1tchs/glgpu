#include <catch2/catch_test_macros.hpp>

#include "test_common.h"

using namespace gpukit;

static Res<Shader> load_pass_shader() {
	auto res = shader_create("tests/assets/test_pass.vert", "tests/assets/test_pass.frag");
	if (res.is_error())
		res = shader_create("../../tests/assets/test_pass.vert", "../../tests/assets/test_pass.frag");
	return res;
}

static Res<Shader> load_compute_shader() {
	auto res = shader_create("tests/assets/test_compute.comp");
	if (res.is_error())
		res = shader_create("../../tests/assets/test_compute.comp");
	return res;
}

TEST_CASE("Graphics Pipeline", "[pipeline][graphics]") {
	gpukit::test::ensure_test_device();

	SECTION("Minimal pipeline with dynamic rendering") {
		auto shader_res = load_pass_shader();
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
		REQUIRE(pipe_res.value() != GL_NULL_HANDLE);
		REQUIRE(pipeline_free(pipe_res.value()).is_ok());
		REQUIRE(shader_free(shader).is_ok());
	}

	SECTION("Pipeline with depth attachment") {
		auto shader_res = load_pass_shader();
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
		REQUIRE(pipeline_free(pipe_res.value()).is_ok());
		REQUIRE(shader_free(shader).is_ok());
	}

	SECTION("Pipeline with alpha blending") {
		auto shader_res = load_pass_shader();
		REQUIRE(shader_res.is_ok());
		Shader shader = shader_res.value();

		GraphicsPipelineCreateInfo pci = {};
		pci.shader = shader;
		pci.primitive = RenderPrimitive::TRIANGLE_LIST;
		pci.rasterization_state.cull_mode = PolygonCullMode::DISABLED;
		pci.multisample_state.sample_count = 1;
		pci.color_blend_state = PipelineColorBlendState::create_blend(1);
		pci.rendering_info.color_attachments = { DataFormat::R8G8B8A8_UNORM };

		auto pipe_res = graphics_pipeline_create(pci);
		REQUIRE(pipe_res.is_ok());
		REQUIRE(pipeline_free(pipe_res.value()).is_ok());
		REQUIRE(shader_free(shader).is_ok());
	}

}

TEST_CASE("Compute Pipeline", "[pipeline][compute]") {
	gpukit::test::ensure_test_device();

	SECTION("Create and free") {
		auto shader_res = load_compute_shader();
		REQUIRE(shader_res.is_ok());
		Shader shader = shader_res.value();

		auto pipe_res = compute_pipeline_create(shader);
		REQUIRE(pipe_res.is_ok());
		REQUIRE(pipe_res.value() != GL_NULL_HANDLE);
		REQUIRE(pipeline_free(pipe_res.value()).is_ok());
		REQUIRE(shader_free(shader).is_ok());
	}

	SECTION("Specialization constant overrides MULTIPLIER") {
		// test_compute.comp declares layout(constant_id = 0) const uint MULTIPLIER = 2
		auto shader_res = load_compute_shader();
		REQUIRE(shader_res.is_ok());
		Shader shader = shader_res.value();

		std::vector<SpecializationConstant> constants = {
			SpecializationConstant::from_uint(0, 3),
		};
		auto pipe_res = compute_pipeline_create(shader, constants);
		REQUIRE(pipe_res.is_ok());
		REQUIRE(pipeline_free(pipe_res.value()).is_ok());
		REQUIRE(shader_free(shader).is_ok());
	}

	SECTION("Full compute dispatch: buffer values are multiplied by 3") {
		auto shader_res = load_compute_shader();
		REQUIRE(shader_res.is_ok());
		Shader shader = shader_res.value();

		std::vector<SpecializationConstant> constants = {
			SpecializationConstant::from_uint(0, 3),
		};
		auto pipe_res = compute_pipeline_create(shader, constants);
		REQUIRE(pipe_res.is_ok());
		Pipeline pipeline = pipe_res.value();

		// Fill storage buffer with 1.0f values
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

		auto queue_res = queue_get(QueueType::COMPUTE);
		REQUIRE(queue_res.is_ok());
		CommandQueue queue = queue_res.value();

		auto pool_res = command_pool_create(queue);
		REQUIRE(pool_res.is_ok());
		CommandPool pool = pool_res.value();

		auto cmd_res = command_pool_allocate(pool);
		REQUIRE(cmd_res.is_ok());
		CommandBuffer cmd = cmd_res.value();

		REQUIRE(command_begin(cmd).is_ok());
		REQUIRE(command_bind_compute_pipeline(cmd, pipeline).is_ok());
		REQUIRE(command_bind_uniform_sets(cmd, shader, 0, set, PipelineType::COMPUTE).is_ok());
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
			REQUIRE(result[i] == 3.0f);
		REQUIRE(buffer_unmap(buf).is_ok());

		REQUIRE(fence_free(fence).is_ok());
		REQUIRE(command_pool_free(pool).is_ok());
		REQUIRE(uniform_set_free(set).is_ok());
		REQUIRE(buffer_free(buf).is_ok());
		REQUIRE(pipeline_free(pipeline).is_ok());
		REQUIRE(shader_free(shader).is_ok());
	}
}
