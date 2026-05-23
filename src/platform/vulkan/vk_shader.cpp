#include "platform/vulkan/vk_device.h"

#include <spirv_reflect.h>
#include <vulkan/vulkan_core.h>
#include <algorithm>
#include <cstdlib>
#include <fstream>

namespace gl {

static Res<std::vector<uint32_t>> _compile_glsl_to_spirv(
		const std::string& source_path, ShaderStageFlags stage) {
	// Generate a temporary spv filename in the same directory
	std::string spv_path = source_path + ".spv";

	// Call glslc
	std::string cmd = "glslc \"" + source_path + "\" -o \"" + spv_path + "\"";
	int ret = std::system(cmd.c_str());
	if (ret != 0) {
		GL_LOG_ERROR("[GLGPU] Shader compilation failed for: {}", source_path);
		return make_err<std::vector<uint32_t>>(Error::SHADER_COMPILATION_FAILED);
	}

	// Load compiled spv
	std::ifstream spv_file(spv_path, std::ios::binary | std::ios::ate);
	if (!spv_file.is_open()) {
		return make_err<std::vector<uint32_t>>(Error::INVALID_ARGUMENT);
	}
	size_t size = spv_file.tellg();
	std::vector<uint32_t> buffer(size / 4);
	spv_file.seekg(0);
	spv_file.read(reinterpret_cast<char*>(buffer.data()), size);
	spv_file.close();

	return buffer;
}

static Res<std::vector<uint32_t>> load_or_compile_shader(
		const std::string& filepath, ShaderStageFlags stage) {
	if (filepath.ends_with(".spv")) {
		std::ifstream file(filepath, std::ios::binary | std::ios::ate);
		if (!file.is_open()) {
			return make_err<std::vector<uint32_t>>(Error::INVALID_ARGUMENT);
		}
		size_t size = file.tellg();
		std::vector<uint32_t> buffer(size / 4);
		file.seekg(0);
		file.read(reinterpret_cast<char*>(buffer.data()), size);
		return buffer;
	} else {
		return _compile_glsl_to_spirv(filepath, stage);
	}
}

static VkDescriptorType _spv_reflect_descriptor_type_to_vk(SpvReflectDescriptorType type) {
	switch (type) {
		case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLER:
			return VK_DESCRIPTOR_TYPE_SAMPLER;
		case SPV_REFLECT_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
			return VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
		case SPV_REFLECT_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
			return VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;
		case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_IMAGE:
			return VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
		case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER:
			return VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER;
		case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER:
			return VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER;
		case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
			return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
		case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER:
			return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
		case SPV_REFLECT_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC:
			return VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC;
		case SPV_REFLECT_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC:
			return VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC;
		case SPV_REFLECT_DESCRIPTOR_TYPE_INPUT_ATTACHMENT:
			return VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
		default:
			return VK_DESCRIPTOR_TYPE_MAX_ENUM;
	}
}

static void _add_descriptor_set_layout_binding_if_not_exists(uint32_t set, uint32_t binding,
		VkDescriptorType type, uint32_t descriptor_count, ShaderStageFlags stage,
		std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>>& bindings) {
	VkShaderStageFlags vk_stage = static_cast<VkShaderStageFlags>(stage);

	const auto it = bindings.find(set);
	if (it != bindings.end()) {
		// set exists look for binding
		std::vector<VkDescriptorSetLayoutBinding>& set_bindings = it->second;

		const auto binding_it = std::find_if(set_bindings.begin(), set_bindings.end(),
				[=](const VkDescriptorSetLayoutBinding& set_binding) -> bool {
					return set_binding.binding == binding;
				});
		if (binding_it != set_bindings.end()) {
			// set, binding already exists now add stage if not exists
			if (!(binding_it->stageFlags & vk_stage)) {
				binding_it->stageFlags |= vk_stage;
			}
			return;
		}
	}

	VkDescriptorSetLayoutBinding layout_binding = {};
	layout_binding.binding = binding;
	layout_binding.descriptorType = type;
	layout_binding.descriptorCount = descriptor_count;
	layout_binding.stageFlags = vk_stage;
	layout_binding.pImmutableSamplers = nullptr;

	bindings[set].push_back(layout_binding);
}

static void _add_push_constant_range_if_not_exists(uint32_t size, uint32_t offset,
		ShaderStageFlags stage, std::vector<VkPushConstantRange>& ranges) {
	VkShaderStageFlags vk_stage = static_cast<VkShaderStageFlags>(stage);

	const auto it = std::find_if(
			ranges.begin(), ranges.end(), [=](const VkPushConstantRange& push_constant) -> bool {
				return push_constant.size == size && push_constant.offset == offset;
			});
	if (it != ranges.end()) {
		// push constant already exists now add the stage
		if (!(it->stageFlags & vk_stage)) {
			it->stageFlags |= vk_stage;
		}
		return;
	}

	VkPushConstantRange range = {};
	range.size = size;
	range.offset = offset;
	range.stageFlags = vk_stage;

	ranges.push_back(range);
}

template <typename T> void _hash_combine(std::size_t& seed, const T& value) {
	std::hash<T> hasher;
	seed ^= hasher(value) + 0x9e3779b9 + (seed << 6) + (seed >> 2);
}

Res<Shader> VulkanDevice::shader_create(VectorView<SpirvEntry> shaders) {
	std::vector<VkShaderModule> vk_shaders;
	std::vector<VkDescriptorSetLayout> descriptor_set_layouts;
	VkPipelineLayout vk_pipeline_layout = VK_NULL_HANDLE;

	// Helper to cleanup resources if we fail halfway through
	auto cleanup_on_failure = [&]() {
		if (vk_pipeline_layout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(_device, vk_pipeline_layout, nullptr);
		}
		for (auto layout : descriptor_set_layouts) {
			vkDestroyDescriptorSetLayout(_device, layout, nullptr);
		}
		for (auto module : vk_shaders) {
			vkDestroyShaderModule(_device, module, nullptr);
		}
	};

	std::map<uint32_t, std::vector<VkDescriptorSetLayoutBinding>> set_bindings;
	// Track which bindings need bindless flags
	std::map<uint32_t, std::map<uint32_t, VkDescriptorBindingFlags>> per_binding_flags;
	std::map<uint32_t, std::map<uint32_t, VulkanDevice::ReflectedBinding>> reflected_bindings;

	std::vector<VkPushConstantRange> push_constant_ranges;
	std::vector<ShaderInterfaceVariable> vertex_input_variables;

	for (const auto& shader : shaders) {
		SpvReflectShaderModule module = {};

		SpvReflectResult result = spvReflectCreateShaderModule(
				shader.byte_code.size() * sizeof(uint32_t), shader.byte_code.data(), &module);

		if (result != SPV_REFLECT_RESULT_SUCCESS) {
			cleanup_on_failure();
			return make_err<Shader>(Error::SHADER_COMPILATION_FAILED);
		}

		// vertex input variables
		if (shader.stage == SHADER_STAGE_VERTEX_BIT) {
			uint32_t input_count = 0;
			if (spvReflectEnumerateInputVariables(&module, &input_count, nullptr) !=
					SPV_REFLECT_RESULT_SUCCESS) {
				spvReflectDestroyShaderModule(&module);
				cleanup_on_failure();
				return make_err<Shader>(Error::SHADER_COMPILATION_FAILED);
			}

			std::vector<SpvReflectInterfaceVariable*> inputs(input_count);
			spvReflectEnumerateInputVariables(&module, &input_count, inputs.data());

			// add input variables
			for (const auto* input : inputs) {
				if (input->name == nullptr || input->location == UINT32_MAX) {
					continue;
				}

				ShaderInterfaceVariable variable;
				variable.name = input->name;
				variable.location = input->location;
				variable.format = static_cast<DataFormat>(input->format);

				vertex_input_variables.push_back(variable);
			}
		}

		// descriptor sets
		{
			uint32_t spv_descriptor_set_count = 0;
			spvReflectEnumerateDescriptorSets(&module, &spv_descriptor_set_count, nullptr);

			std::vector<SpvReflectDescriptorSet*> spv_descriptor_sets(spv_descriptor_set_count);
			spvReflectEnumerateDescriptorSets(
					&module, &spv_descriptor_set_count, spv_descriptor_sets.data());

			// add bindings
			for (const auto* descriptor_set : spv_descriptor_sets) {
				for (uint32_t i = 0; i < descriptor_set->binding_count; i++) {
					const SpvReflectDescriptorBinding* binding = descriptor_set->bindings[i];

					uint32_t count = binding->count;

					bool is_runtime_array =
							(binding->type_description->traits.array.dims_count > 0 &&
									binding->type_description->traits.array.dims[0] == 0);

					bool is_explicitly_named = binding->name != nullptr
							? std::string(binding->name).starts_with("h_")
							: false;

					if (is_runtime_array || is_explicitly_named) {
						count = get_max_bindless_instances();

						per_binding_flags[descriptor_set->set][binding->binding] =
								VK_DESCRIPTOR_BINDING_PARTIALLY_BOUND_BIT |
								VK_DESCRIPTOR_BINDING_UPDATE_AFTER_BIND_BIT;
					}

					_add_descriptor_set_layout_binding_if_not_exists(descriptor_set->set,
							binding->binding,
							_spv_reflect_descriptor_type_to_vk(binding->descriptor_type), count,
							shader.stage, set_bindings);

					VulkanDevice::ReflectedBinding reflected = {};
					reflected.binding = binding->binding;
					reflected.type = _spv_reflect_descriptor_type_to_vk(binding->descriptor_type);
					reflected.count = count;
					reflected.name = binding->name ? binding->name : "";

					reflected_bindings[descriptor_set->set][binding->binding] = reflected;
				}
			}
		}

		// push constants
		{
			uint32_t spv_push_constant_count = 0;
			spvReflectEnumeratePushConstantBlocks(&module, &spv_push_constant_count, nullptr);

			std::vector<SpvReflectBlockVariable*> spv_push_constants(spv_push_constant_count);
			spvReflectEnumeratePushConstantBlocks(
					&module, &spv_push_constant_count, spv_push_constants.data());

			for (const auto* push_constant : spv_push_constants) {
				_add_push_constant_range_if_not_exists(push_constant->size, push_constant->offset,
						shader.stage, push_constant_ranges);
			}
		}

		// create a new shader module, using the buffer we loaded
		VkShaderModuleCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
		create_info.pNext = nullptr;
		create_info.codeSize = shader.byte_code.size() * sizeof(uint32_t);
		create_info.pCode = shader.byte_code.data();

		VkShaderModule vk_shader = VK_NULL_HANDLE;
		VkResult mod_res = vkCreateShaderModule(_device, &create_info, nullptr, &vk_shader);

		spvReflectDestroyShaderModule(
				&module); // Destroy reflect module before checking vulkan result

		if (mod_res != VK_SUCCESS) {
			cleanup_on_failure();
			return make_err<Shader>(Error::SHADER_COMPILATION_FAILED);
		}

		vk_shaders.push_back(vk_shader);
	}

	for (auto& [set_index, bindings] : set_bindings) {
		std::sort(bindings.begin(), bindings.end(),
				[=](const VkDescriptorSetLayoutBinding& lhs,
						const VkDescriptorSetLayoutBinding& rhs) -> bool {
					return lhs.binding < rhs.binding;
				});

		std::vector<VkDescriptorBindingFlags> binding_flags_list;
		bool is_set_bindless = false;

		for (const auto& b : bindings) {
			// Look up if this specific binding was marked as bindless
			auto it = per_binding_flags[set_index].find(b.binding);
			if (it != per_binding_flags[set_index].end()) {
				binding_flags_list.push_back(it->second);
				is_set_bindless = true;
			} else {
				binding_flags_list.push_back(0);
			}
		}

		VkDescriptorSetLayoutCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
		create_info.bindingCount = static_cast<uint32_t>(bindings.size());
		create_info.pBindings = bindings.data();

		VkDescriptorSetLayoutBindingFlagsCreateInfo flags_info = {};
		if (is_set_bindless) {
			flags_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO;
			flags_info.bindingCount = static_cast<uint32_t>(binding_flags_list.size());
			flags_info.pBindingFlags = binding_flags_list.data();

			create_info.pNext = &flags_info;
			create_info.flags = VK_DESCRIPTOR_SET_LAYOUT_CREATE_UPDATE_AFTER_BIND_POOL_BIT;
		}

		VkDescriptorSetLayout vk_set = VK_NULL_HANDLE;
		if (vkCreateDescriptorSetLayout(_device, &create_info, nullptr, &vk_set) != VK_SUCCESS) {
			cleanup_on_failure();
			return make_err<Shader>(Error::INITIALIZATION_FAILED);
		}

		descriptor_set_layouts.push_back(vk_set);
	}

	VkPipelineLayoutCreateInfo pipeline_layout_info = {};
	pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
	pipeline_layout_info.setLayoutCount = static_cast<uint32_t>(descriptor_set_layouts.size());
	pipeline_layout_info.pSetLayouts = descriptor_set_layouts.data();
	pipeline_layout_info.pushConstantRangeCount =
			static_cast<uint32_t>(push_constant_ranges.size());
	pipeline_layout_info.pPushConstantRanges = push_constant_ranges.data();

	if (vkCreatePipelineLayout(_device, &pipeline_layout_info, nullptr, &vk_pipeline_layout) !=
			VK_SUCCESS) {
		cleanup_on_failure();
		return make_err<Shader>(Error::INITIALIZATION_FAILED);
	}

	std::vector<VkPipelineShaderStageCreateInfo> shader_stages;
	for (size_t i = 0; i < shaders.size(); i++) {
		VkPipelineShaderStageCreateInfo create_info = {};
		create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
		create_info.stage = static_cast<VkShaderStageFlagBits>(shaders[i].stage);
		create_info.pName = "main";
		create_info.module = vk_shaders[i];

		shader_stages.push_back(create_info);
	}

	uint32_t push_constant_stages = 0;
	for (const auto& push_constant : push_constant_ranges) {
		push_constant_stages |= push_constant.stageFlags;
	}

	// prepare hash
	size_t shader_hash = 0;
	{
		for (const auto& shader : shaders) {
			_hash_combine(shader_hash, shader.stage);
			_hash_combine(shader_hash, shader.byte_code.size());
		}
		for (const auto& [_, bindings] : set_bindings) {
			for (const auto& binding : bindings) {
				_hash_combine(shader_hash, binding.binding);
				_hash_combine(shader_hash, binding.stageFlags);
				_hash_combine(shader_hash, binding.descriptorCount);
				_hash_combine(shader_hash, binding.descriptorType);
			}
		}
		for (const auto& push_constant : push_constant_ranges) {
			_hash_combine(shader_hash, push_constant.stageFlags);
			_hash_combine(shader_hash, push_constant.offset);
			_hash_combine(shader_hash, push_constant.size);
		}
	}

	// Bookkeep
	VulkanShader* shader_info = VersatileResource::allocate<VulkanShader>(_resources_allocator);
	if (!shader_info) {
		cleanup_on_failure();
		return make_err<Shader>(Error::OUT_OF_HOST_MEMORY);
	}

	shader_info->stage_create_infos = shader_stages;
	shader_info->push_constant_stages = push_constant_stages;
	shader_info->descriptor_set_layouts = descriptor_set_layouts;
	shader_info->pipeline_layout = vk_pipeline_layout;
	shader_info->vertex_input_variables = vertex_input_variables;
	shader_info->shader_hash = shader_hash;
	shader_info->reflected_bindings = reflected_bindings;

	return Shader(shader_info);
}

Res<> VulkanDevice::shader_free(Shader shader) {
	if (!shader) {
		return {};
	}

	VulkanShader* shader_info = (VulkanShader*)shader;

	for (size_t i = 0; i < shader_info->descriptor_set_layouts.size(); i++) {
		vkDestroyDescriptorSetLayout(_device, shader_info->descriptor_set_layouts[i], nullptr);
	}

	vkDestroyPipelineLayout(_device, shader_info->pipeline_layout, nullptr);

	for (size_t i = 0; i < shader_info->stage_create_infos.size(); i++) {
		vkDestroyShaderModule(_device, shader_info->stage_create_infos[i].module, nullptr);
	}

	VersatileResource::free(_resources_allocator, shader_info);

	return {};
}

Res<std::vector<ShaderInterfaceVariable>> VulkanDevice::shader_get_vertex_inputs(Shader shader) {
	if (!shader) {
		return make_err<std::vector<ShaderInterfaceVariable>>(Error::INVALID_HANDLE);
	}

	VulkanShader* shader_info = (VulkanShader*)shader;
	return shader_info->vertex_input_variables;
}

Res<Shader> VulkanDevice::shader_create(
		const char* vertex_filepath, const char* fragment_filepath) {
	if (!vertex_filepath || !fragment_filepath) {
		return make_err<Shader>(Error::INVALID_ARGUMENT);
	}

	auto vert_bytecode_res = load_or_compile_shader(vertex_filepath, SHADER_STAGE_VERTEX_BIT);
	if (vert_bytecode_res.is_error()) {
		return make_err<Shader>(vert_bytecode_res.error());
	}

	auto frag_bytecode_res = load_or_compile_shader(fragment_filepath, SHADER_STAGE_FRAGMENT_BIT);
	if (frag_bytecode_res.is_error()) {
		return make_err<Shader>(frag_bytecode_res.error());
	}

	SpirvEntry vert_entry{ .byte_code = vert_bytecode_res.value(),
		.stage = SHADER_STAGE_VERTEX_BIT };

	SpirvEntry frag_entry{ .byte_code = frag_bytecode_res.value(),
		.stage = SHADER_STAGE_FRAGMENT_BIT };

	std::vector<SpirvEntry> entries = { vert_entry, frag_entry };
	return shader_create(entries);
}

Res<Shader> VulkanDevice::shader_create(const char* compute_filepath) {
	if (!compute_filepath) {
		return make_err<Shader>(Error::INVALID_ARGUMENT);
	}

	auto comp_bytecode_res = load_or_compile_shader(compute_filepath, SHADER_STAGE_COMPUTE_BIT);
	if (comp_bytecode_res.is_error()) {
		return make_err<Shader>(comp_bytecode_res.error());
	}

	SpirvEntry comp_entry{ .byte_code = comp_bytecode_res.value(),
		.stage = SHADER_STAGE_COMPUTE_BIT };

	std::vector<SpirvEntry> entries = { comp_entry };
	return shader_create(entries);
}

Res<std::vector<ShaderResourceInfo>> VulkanDevice::shader_get_resources(Shader shader) {
	if (!shader) {
		return make_err<std::vector<ShaderResourceInfo>>(Error::INVALID_HANDLE);
	}

	VulkanShader* shader_info = (VulkanShader*)shader;
	std::vector<ShaderResourceInfo> resources;

	for (auto& [set_index, bindings] : shader_info->reflected_bindings) {
		for (auto& [binding_index, binding] : bindings) {
			ShaderResourceInfo info = {};
			info.set = set_index;
			info.binding = binding.binding;
			info.name = binding.name;
			info.count = binding.count;

			switch (binding.type) {
				case VK_DESCRIPTOR_TYPE_SAMPLER:
					info.type = ShaderUniformType::SAMPLER;
					break;
				case VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:
					info.type = ShaderUniformType::SAMPLER_WITH_TEXTURE;
					break;
				case VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:
					info.type = ShaderUniformType::TEXTURE;
					break;
				case VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:
					info.type = ShaderUniformType::IMAGE;
					break;
				case VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:
					info.type = ShaderUniformType::UNIFORM_BUFFER;
					break;
				case VK_DESCRIPTOR_TYPE_STORAGE_BUFFER:
					info.type = ShaderUniformType::STORAGE_BUFFER;
					break;
				default:
					continue;
			}
			resources.push_back(info);
		}
	}

	return resources;
}

} //namespace gl
