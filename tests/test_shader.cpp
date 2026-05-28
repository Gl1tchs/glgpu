#include <catch2/catch_test_macros.hpp>

#include "test_common.h"

using namespace gpukit;

static Res<Shader> load_bindless_shader() {
	auto res = shader_create(
			"tests/assets/test_bindless.vert", "tests/assets/test_bindless.frag");
	if (res.is_error())
		res = shader_create(
				"../../tests/assets/test_bindless.vert", "../../tests/assets/test_bindless.frag");
	return res;
}

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

TEST_CASE("Shader", "[shader]") {
	gpukit::test::ensure_test_device();

	SECTION("Create and free graphics shader") {
		auto res = load_pass_shader();
		REQUIRE(res.is_ok());
		Shader shader = res.value();
		REQUIRE(shader != GL_NULL_HANDLE);
		REQUIRE(shader_free(shader).is_ok());
	}

	SECTION("Create and free compute shader") {
		auto res = load_compute_shader();
		REQUIRE(res.is_ok());
		Shader shader = res.value();
		REQUIRE(shader != GL_NULL_HANDLE);
		REQUIRE(shader_free(shader).is_ok());
	}

	SECTION("Invalid path returns error") {
		auto res = shader_create("nonexistent.vert", "nonexistent.frag");
		REQUIRE(res.is_error());
	}

	SECTION("Invalid compute path returns error") {
		auto res = shader_create("nonexistent.comp");
		REQUIRE(res.is_error());
	}

	SECTION("Vertex input reflection") {
		// test_bindless.vert has location=0 (in_pos vec2) and location=1 (in_uv vec2)
		auto shader_res = load_bindless_shader();
		REQUIRE(shader_res.is_ok());
		Shader shader = shader_res.value();

		auto inputs_res = shader_get_vertex_inputs(shader);
		REQUIRE(inputs_res.is_ok());
		const auto& inputs = inputs_res.value();
		REQUIRE(inputs.size() >= 2);

		bool has_location0 = false;
		bool has_location1 = false;
		for (const auto& v : inputs) {
			if (v.location == 0)
				has_location0 = true;
			if (v.location == 1)
				has_location1 = true;
		}
		REQUIRE(has_location0);
		REQUIRE(has_location1);

		REQUIRE(shader_free(shader).is_ok());
	}

	SECTION("No vertex inputs on passthrough shader") {
		auto shader_res = load_pass_shader();
		REQUIRE(shader_res.is_ok());
		Shader shader = shader_res.value();

		auto inputs_res = shader_get_vertex_inputs(shader);
		REQUIRE(inputs_res.is_ok());
		REQUIRE(inputs_res.value().empty());

		REQUIRE(shader_free(shader).is_ok());
	}

	SECTION("Resource reflection: bindless shader has all binding types") {
		auto shader_res = load_bindless_shader();
		REQUIRE(shader_res.is_ok());
		Shader shader = shader_res.value();

		auto resources_res = shader_get_resources(shader);
		REQUIRE(resources_res.is_ok());
		const auto& resources = resources_res.value();
		REQUIRE(!resources.empty());

		bool has_texture = false;
		bool has_sampled_image = false;
		bool has_sampler = false;
		bool has_storage_image = false;
		bool has_storage_buffer = false;

		for (const auto& r : resources) {
			if (r.set != 0)
				continue;
			switch (r.binding) {
				case 0: has_texture = (r.type == ShaderUniformType::SAMPLER_WITH_TEXTURE); break;
				case 1: has_sampled_image = (r.type == ShaderUniformType::TEXTURE); break;
				case 2: has_sampler = (r.type == ShaderUniformType::SAMPLER); break;
				case 3: has_storage_image = (r.type == ShaderUniformType::IMAGE); break;
				case 4: has_storage_buffer = (r.type == ShaderUniformType::STORAGE_BUFFER); break;
			}
		}

		REQUIRE(has_texture);
		REQUIRE(has_sampled_image);
		REQUIRE(has_sampler);
		REQUIRE(has_storage_image);
		REQUIRE(has_storage_buffer);

		REQUIRE(shader_free(shader).is_ok());
	}

	SECTION("Resource reflection: compute shader exposes storage buffer at set=0 binding=0") {
		auto shader_res = load_compute_shader();
		REQUIRE(shader_res.is_ok());
		Shader shader = shader_res.value();

		auto resources_res = shader_get_resources(shader);
		REQUIRE(resources_res.is_ok());
		const auto& resources = resources_res.value();
		REQUIRE(!resources.empty());

		bool found = false;
		for (const auto& r : resources) {
			if (r.set == 0 && r.binding == 0 && r.type == ShaderUniformType::STORAGE_BUFFER) {
				found = true;
				break;
			}
		}
		REQUIRE(found);

		REQUIRE(shader_free(shader).is_ok());
	}

}
