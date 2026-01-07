#include "platform/vulkan/vk_device.h"

#include "platform/vulkan/vk_common.h"

#include <filesystem>
#include <fstream>

namespace gl {

static VkCullModeFlagBits _gl_to_vk_cull_mode(PolygonCullMode cull_mode) {
	switch (cull_mode) {
		case PolygonCullMode::DISABLED:
			return VK_CULL_MODE_NONE;
		case PolygonCullMode::FRONT:
			return VK_CULL_MODE_FRONT_BIT;
		case PolygonCullMode::BACK:
			return VK_CULL_MODE_BACK_BIT;
		default:
			return VK_CULL_MODE_FLAG_BITS_MAX_ENUM;
	}
}

constexpr uint32_t PIPELINE_CACHE_MAGIC_NUMBER = 0xbba786cf;

struct PipelineCacheHeader {
	uint32_t magic_number; // PIPELINE_MAGIC_NUMBER
	size_t data_size; // size of the data
	uint32_t vendor_id; // VkPhysicalDeviceProperties::vendorID
	uint32_t device_id; // VkPhysicalDeviceProperties::deviceID
	uint32_t driver_version; // VkPhysicalDeviceProperties::driverVersion
	uint8_t uuid[VK_UUID_SIZE]; // VkPhysicalDeviceProperties::pipelineCacheUUID
};

// Returns a valid cache or VK_NULL_HANDLE if not found/invalid.
// Returns Error only on API failure (OOM).
static Res<VkPipelineCache> _load_pipeline_cache(VkDevice device, const std::filesystem::path& path,
		const VkPhysicalDeviceProperties& device_props) {
	VkPipelineCacheCreateInfo cache_create_info = {};
	cache_create_info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;

	// Data buffer to hold file content
	std::vector<char> cache_buffer;

	// if cache already exists on disk try to load it
	if (std::filesystem::exists(path)) {
		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if (file) {
			const size_t file_size = file.tellg();
			file.seekg(0, std::ios::beg);

			cache_buffer.resize(file_size);
			file.read(cache_buffer.data(), file_size);
			file.close();

			// Validate header
			if (file_size > sizeof(PipelineCacheHeader)) {
				const PipelineCacheHeader* header =
						reinterpret_cast<const PipelineCacheHeader*>(cache_buffer.data());

				int64_t data_size = file_size - sizeof(PipelineCacheHeader);

				bool valid = true;
				if (header->magic_number != PIPELINE_CACHE_MAGIC_NUMBER)
					valid = false;
				if (header->data_size != data_size)
					valid = false;
				if (header->vendor_id != device_props.vendorID)
					valid = false;
				if (header->device_id != device_props.deviceID)
					valid = false;
				if (header->driver_version != device_props.driverVersion)
					valid = false;
				if (memcmp(header->uuid, device_props.pipelineCacheUUID, VK_UUID_SIZE) != 0)
					valid = false;

				if (valid) {
					cache_create_info.initialDataSize = data_size;
					cache_create_info.pInitialData =
							cache_buffer.data() + sizeof(PipelineCacheHeader);
				} else {
					GL_LOG_WARNING("[VULKAN] Pipeline cache invalid or outdated, creating new.");
				}
			}
		} else {
			GL_LOG_ERROR("[VULKAN] Unable to read pipeline cache at '{}'", path.string());
		}
	}

	VkPipelineCache vk_pipeline_cache = VK_NULL_HANDLE;
	VK_CHECK_RET(vkCreatePipelineCache(device, &cache_create_info, nullptr, &vk_pipeline_cache),
			make_err<VkPipelineCache>(Error::OUT_OF_HOST_MEMORY));

	return vk_pipeline_cache;
}

static VkPipelineVertexInputStateCreateInfo _get_vertex_input_state_info(
		VulkanDevice* backend, Shader shader, PipelineVertexInputState vertex_input_state) {
	VkPipelineVertexInputStateCreateInfo vertex_info = {};
	vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;

	return vertex_info;
}

static VkPipelineInputAssemblyStateCreateInfo _get_input_assembly_state_info(
		RenderPrimitive render_primitive) {
	VkPipelineInputAssemblyStateCreateInfo input_assembly = {};
	input_assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
	input_assembly.topology = static_cast<VkPrimitiveTopology>(render_primitive);
	input_assembly.primitiveRestartEnable = false;
	return input_assembly;
}

static VkPipelineRasterizationStateCreateInfo _get_rasterization_state_info(
		PipelineRasterizationState rasterization_state) {
	VkPipelineRasterizationStateCreateInfo rasterizer = {};
	rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
	rasterizer.depthClampEnable = rasterization_state.enable_depth_clamp;
	rasterizer.rasterizerDiscardEnable = rasterization_state.discard_primitives;
	rasterizer.polygonMode =
			rasterization_state.wireframe ? VK_POLYGON_MODE_LINE : VK_POLYGON_MODE_FILL;
	rasterizer.cullMode = _gl_to_vk_cull_mode(rasterization_state.cull_mode);
	rasterizer.frontFace = rasterization_state.front_face == PolygonFrontFace::CLOCKWISE
			? VK_FRONT_FACE_CLOCKWISE
			: VK_FRONT_FACE_COUNTER_CLOCKWISE;
	rasterizer.depthBiasEnable = rasterization_state.depth_bias_enabled;
	rasterizer.depthBiasClamp = rasterization_state.depth_bias_clamp;
	rasterizer.depthBiasSlopeFactor = rasterization_state.depth_bias_slope_factor;
	rasterizer.lineWidth = rasterization_state.line_width;
	return rasterizer;
}

static VkPipelineMultisampleStateCreateInfo _get_multisampling_state_info(
		PipelineMultisampleState multisample_state) {
	VkPipelineMultisampleStateCreateInfo multisampling = {};
	multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
	multisampling.rasterizationSamples =
			static_cast<VkSampleCountFlagBits>(multisample_state.sample_count);
	multisampling.sampleShadingEnable = multisample_state.enable_sample_shading;
	multisampling.minSampleShading = multisample_state.min_sample_shading;
	// Ptr to sample mask must persist!
	multisampling.pSampleMask = (VkSampleMask*)multisample_state.sample_mask.data();
	multisampling.alphaToCoverageEnable = multisample_state.enable_alpha_to_coverage;
	multisampling.alphaToOneEnable = multisample_state.enable_alpha_to_one;
	return multisampling;
}

static VkPipelineDepthStencilStateCreateInfo _get_depth_stencil_state_info(
		PipelineDepthStencilState depth_stencil_state) {
	VkPipelineDepthStencilStateCreateInfo depth_stencil = {};
	depth_stencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
	depth_stencil.depthTestEnable = depth_stencil_state.enable_depth_test;
	depth_stencil.depthWriteEnable = depth_stencil_state.enable_depth_write;
	depth_stencil.depthCompareOp =
			static_cast<VkCompareOp>(depth_stencil_state.depth_compare_operator);
	depth_stencil.depthBoundsTestEnable = depth_stencil_state.enable_depth_range;
	depth_stencil.minDepthBounds = depth_stencil_state.depth_range_min;
	depth_stencil.maxDepthBounds = depth_stencil_state.depth_range_max;
	depth_stencil.stencilTestEnable = depth_stencil_state.enable_stencil;

	VkStencilOpState front;
	front.failOp = static_cast<VkStencilOp>(depth_stencil_state.front_op.fail);
	front.passOp = static_cast<VkStencilOp>(depth_stencil_state.front_op.pass);
	front.depthFailOp = static_cast<VkStencilOp>(depth_stencil_state.front_op.depth_fail);
	front.compareOp = static_cast<VkCompareOp>(depth_stencil_state.front_op.compare);
	front.compareMask = depth_stencil_state.front_op.compare_mask;
	front.writeMask = depth_stencil_state.front_op.write_mask;
	front.reference = depth_stencil_state.front_op.reference;
	depth_stencil.front = front;

	VkStencilOpState back;
	back.failOp = static_cast<VkStencilOp>(depth_stencil_state.back_op.fail);
	back.passOp = static_cast<VkStencilOp>(depth_stencil_state.back_op.pass);
	back.depthFailOp = static_cast<VkStencilOp>(depth_stencil_state.back_op.depth_fail);
	back.compareOp = static_cast<VkCompareOp>(depth_stencil_state.back_op.compare);
	back.compareMask = depth_stencil_state.back_op.compare_mask;
	back.writeMask = depth_stencil_state.back_op.write_mask;
	back.reference = depth_stencil_state.back_op.reference;
	depth_stencil.back = back;

	return depth_stencil;
}

static std::vector<VkPipelineColorBlendAttachmentState> _get_color_blend_attachments(
		PipelineColorBlendState blend_state) {
	std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachments;
	for (const auto& attachment : blend_state.attachments) {
		VkPipelineColorBlendAttachmentState vk_attachment = {};
		vk_attachment.blendEnable = attachment.enable_blend;
		vk_attachment.srcColorBlendFactor =
				static_cast<VkBlendFactor>(attachment.src_color_blend_factor);
		vk_attachment.dstColorBlendFactor =
				static_cast<VkBlendFactor>(attachment.dst_color_blend_factor);
		vk_attachment.colorBlendOp = static_cast<VkBlendOp>(attachment.color_blend_op);
		vk_attachment.srcAlphaBlendFactor =
				static_cast<VkBlendFactor>(attachment.src_alpha_blend_factor);
		vk_attachment.dstAlphaBlendFactor =
				static_cast<VkBlendFactor>(attachment.dst_alpha_blend_factor);
		vk_attachment.alphaBlendOp = static_cast<VkBlendOp>(attachment.alpha_blend_op);

		if (attachment.write_r)
			vk_attachment.colorWriteMask |= VK_COLOR_COMPONENT_R_BIT;
		if (attachment.write_g)
			vk_attachment.colorWriteMask |= VK_COLOR_COMPONENT_G_BIT;
		if (attachment.write_b)
			vk_attachment.colorWriteMask |= VK_COLOR_COMPONENT_B_BIT;
		if (attachment.write_a)
			vk_attachment.colorWriteMask |= VK_COLOR_COMPONENT_A_BIT;

		color_blend_attachments.push_back(vk_attachment);
	}
	return color_blend_attachments;
}

static VkPipelineColorBlendStateCreateInfo _get_color_blend_state_info(
		PipelineColorBlendState blend_state,
		const std::vector<VkPipelineColorBlendAttachmentState>& attachments) {
	VkPipelineColorBlendStateCreateInfo color_blend = {};
	color_blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
	color_blend.logicOpEnable = blend_state.enable_logic_op;
	color_blend.logicOp = static_cast<VkLogicOp>(blend_state.logic_op);
	color_blend.attachmentCount = static_cast<uint32_t>(attachments.size());
	color_blend.pAttachments = attachments.data();
	color_blend.blendConstants[0] = blend_state.blend_constant.x;
	color_blend.blendConstants[1] = blend_state.blend_constant.y;
	color_blend.blendConstants[2] = blend_state.blend_constant.z;
	color_blend.blendConstants[3] = blend_state.blend_constant.w;
	return color_blend;
}

static std::vector<VkDynamicState> _get_dynamic_states(PipelineDynamicStateFlags dynamic_state) {
	std::vector<VkDynamicState> states = {
		VK_DYNAMIC_STATE_VIEWPORT,
		VK_DYNAMIC_STATE_SCISSOR,
	};
	if (dynamic_state & PIPELINE_DYNAMIC_STATE_LINE_WIDTH)
		states.push_back(VK_DYNAMIC_STATE_LINE_WIDTH);
	if (dynamic_state & PIPELINE_DYNAMIC_STATE_DEPTH_BIAS)
		states.push_back(VK_DYNAMIC_STATE_DEPTH_BIAS);
	if (dynamic_state & PIPELINE_DYNAMIC_STATE_BLEND_CONSTANTS)
		states.push_back(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
	if (dynamic_state & PIPELINE_DYNAMIC_STATE_DEPTH_BOUNDS)
		states.push_back(VK_DYNAMIC_STATE_DEPTH_BOUNDS);
	if (dynamic_state & PIPELINE_DYNAMIC_STATE_STENCIL_COMPARE_MASK)
		states.push_back(VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK);
	if (dynamic_state & PIPELINE_DYNAMIC_STATE_STENCIL_WRITE_MASK)
		states.push_back(VK_DYNAMIC_STATE_STENCIL_WRITE_MASK);
	if (dynamic_state & PIPELINE_DYNAMIC_STATE_STENCIL_REFERENCE)
		states.push_back(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
	return states;
}

static VkPipelineDynamicStateCreateInfo _get_dynamic_state_info(
		const std::vector<VkDynamicState>& states) {
	VkPipelineDynamicStateCreateInfo dynamic_state = {};
	dynamic_state.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
	dynamic_state.dynamicStateCount = static_cast<uint32_t>(states.size());
	dynamic_state.pDynamicStates = states.data();
	return dynamic_state;
}

constexpr static VkPipelineViewportStateCreateInfo _get_viewport_state() {
	VkPipelineViewportStateCreateInfo viewport_state = {};
	viewport_state.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
	viewport_state.viewportCount = 1;
	viewport_state.scissorCount = 1;
	return viewport_state;
}

Res<Pipeline> VulkanDevice::graphics_pipeline_create(const GraphicsPipelineCreateInfo& info) {
	VulkanShader* shader = (VulkanShader*)info.shader;
	if (!shader) {
		return make_err<Pipeline>(Error::INVALID_ARGUMENT);
	}

	VkVertexInputBindingDescription vertex_binding = {};
	std::vector<VkVertexInputAttributeDescription> vertex_attributes;

	if (info.vertex_input_state.stride != 0) {
		vertex_binding.binding = 0;
		vertex_binding.stride = info.vertex_input_state.stride;
		vertex_binding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

		auto vertex_inputs_res = shader_get_vertex_inputs(info.shader);
		// NOTE: if shader analysis fails, we might proceed with empty or error out
		if (vertex_inputs_res.is_error()) {
			return make_err<Pipeline>(vertex_inputs_res.error());
		}

		std::vector<ShaderInterfaceVariable> inputs = vertex_inputs_res.value();

		for (const auto& input : inputs) {
			VkVertexInputAttributeDescription attribute = {};
			attribute.binding = 0;
			attribute.format = static_cast<VkFormat>(input.format);
			attribute.location = input.location;
			attribute.offset = 0;
			vertex_attributes.push_back(attribute);
		}

		std::sort(vertex_attributes.begin(), vertex_attributes.end(),
				[](const auto& lhs, const auto& rhs) { return lhs.location < rhs.location; });

		uint32_t offset = 0;
		for (auto& attr : vertex_attributes) {
			size_t data_size = get_data_format_size(static_cast<DataFormat>(attr.format));
			if (data_size == 0)
				return make_err<Pipeline>(Error::INVALID_ARGUMENT); // Unsupported format

			attr.offset = offset;
			offset += data_size;
		}
	}

	VkPipelineVertexInputStateCreateInfo vertex_info = {};
	vertex_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
	vertex_info.vertexBindingDescriptionCount = info.vertex_input_state.stride != 0 ? 1 : 0;
	vertex_info.pVertexBindingDescriptions = &vertex_binding;
	vertex_info.vertexAttributeDescriptionCount = static_cast<uint32_t>(vertex_attributes.size());
	vertex_info.pVertexAttributeDescriptions = vertex_attributes.data();

	const VkPipelineInputAssemblyStateCreateInfo input_assembly =
			_get_input_assembly_state_info(info.primitive);
	const VkPipelineRasterizationStateCreateInfo rasterizer =
			_get_rasterization_state_info(info.rasterization_state);
	const VkPipelineMultisampleStateCreateInfo multisampling =
			_get_multisampling_state_info(info.multisample_state);
	const VkPipelineDepthStencilStateCreateInfo depth_stencil =
			_get_depth_stencil_state_info(info.depth_stencil_state);
	const std::vector<VkPipelineColorBlendAttachmentState> color_blend_attachments =
			_get_color_blend_attachments(info.color_blend_state);
	const VkPipelineColorBlendStateCreateInfo color_blend =
			_get_color_blend_state_info(info.color_blend_state, color_blend_attachments);

	const std::vector<VkDynamicState> dynamic_states = _get_dynamic_states(info.dynamic_state);
	const VkPipelineDynamicStateCreateInfo dynamic_state = _get_dynamic_state_info(dynamic_states);

	const VkPipelineViewportStateCreateInfo viewport_state = _get_viewport_state();

	// Target Dependency Handling (RenderPass vs. Dynamic Rendering)
	VkPipelineRenderingCreateInfo rendering_info = {};
	void* p_next_chain = nullptr;

	if (info.render_pass == GL_NULL_HANDLE) {
		rendering_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
		rendering_info.colorAttachmentCount =
				static_cast<uint32_t>(info.rendering_info.color_attachments.size());
		rendering_info.pColorAttachmentFormats =
				(VkFormat*)info.rendering_info.color_attachments.data();
		rendering_info.depthAttachmentFormat =
				static_cast<VkFormat>(info.rendering_info.depth_attachment);

		p_next_chain = &rendering_info;
	}

	VkGraphicsPipelineCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
	create_info.pNext = p_next_chain;

	if (info.render_pass != GL_NULL_HANDLE) {
		VulkanRenderPass* render_pass = (VulkanRenderPass*)info.render_pass;
		create_info.renderPass = render_pass->vk_render_pass;
		create_info.subpass = 0;
	}

	create_info.stageCount = static_cast<uint32_t>(shader->stage_create_infos.size());
	create_info.pStages = shader->stage_create_infos.data();
	create_info.pVertexInputState = &vertex_info;
	create_info.pInputAssemblyState = &input_assembly;
	create_info.pViewportState = &viewport_state;
	create_info.pRasterizationState = &rasterizer;
	create_info.pMultisampleState = &multisampling;
	create_info.pDepthStencilState = &depth_stencil;
	create_info.pColorBlendState = &color_blend;
	create_info.pDynamicState = &dynamic_state;
	create_info.layout = shader->pipeline_layout;

	// Pipeline Cache
	const auto tmp = std::filesystem::temp_directory_path();
	const auto cache_path = tmp / std::format("glitch/cache/{}.cache", shader->shader_hash);

	Res<VkPipelineCache> cache_res =
			_load_pipeline_cache(_device, cache_path, _physical_device_properties);
	// If cache creation fails (OOM), we propagate the error.
	// If cache just didn't exist, it returned VK_NULL_HANDLE (valid).
	if (cache_res.is_error()) {
		return make_err<Pipeline>(cache_res.error());
	}

	VkPipelineCache vk_pipeline_cache = cache_res.value();

	VkPipeline vk_pipeline = VK_NULL_HANDLE;
	VK_CHECK_RET(vkCreateGraphicsPipelines(
						 _device, vk_pipeline_cache, 1, &create_info, nullptr, &vk_pipeline),
			make_err<Pipeline>(Error::PIPELINE_CREATION_FAILED));

	VulkanPipeline* pipeline = VersatileResource::allocate<VulkanPipeline>(_resources_allocator);
	pipeline->vk_pipeline = vk_pipeline;
	pipeline->vk_pipeline_cache = vk_pipeline_cache;
	pipeline->shader_hash = shader->shader_hash;

	return Pipeline(pipeline);
}

Res<Pipeline> VulkanDevice::compute_pipeline_create(Shader shader) {
	VulkanShader* vk_shader = (VulkanShader*)shader;
	if (!vk_shader) {
		return make_err<Pipeline>(Error::INVALID_ARGUMENT);
	}

	VkComputePipelineCreateInfo create_info = {};
	create_info.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
	create_info.stage = vk_shader->stage_create_infos[0];
	create_info.layout = vk_shader->pipeline_layout;

	const auto tmp = std::filesystem::temp_directory_path();
	const auto cache_path = tmp / std::format("glitch/cache/{}.cache", vk_shader->shader_hash);

	Res<VkPipelineCache> cache_res =
			_load_pipeline_cache(_device, cache_path, _physical_device_properties);
	if (cache_res.is_error()) {
		return make_err<Pipeline>(cache_res.error());
	}

	VkPipelineCache vk_pipeline_cache = cache_res.value();

	VkPipeline vk_pipeline = VK_NULL_HANDLE;
	VK_CHECK_RET(vkCreateComputePipelines(
						 _device, vk_pipeline_cache, 1, &create_info, nullptr, &vk_pipeline),
			make_err<Pipeline>(Error::PIPELINE_CREATION_FAILED));

	VulkanPipeline* pipeline = VersatileResource::allocate<VulkanPipeline>(_resources_allocator);
	pipeline->vk_pipeline = vk_pipeline;
	pipeline->vk_pipeline_cache = vk_pipeline_cache;
	pipeline->shader_hash = vk_shader->shader_hash;

	return Pipeline(pipeline);
}

Res<> VulkanDevice::pipeline_free(Pipeline pipeline) {
	if (!pipeline) {
		return {};
	}

	VulkanPipeline* vk_pipeline = (VulkanPipeline*)pipeline;

	// Save the pipeline cache
	if (vk_pipeline->vk_pipeline_cache != VK_NULL_HANDLE) {
		size_t cache_size = 0;
		VkResult res = vkGetPipelineCacheData(
				_device, vk_pipeline->vk_pipeline_cache, &cache_size, nullptr);

		if (res == VK_SUCCESS && cache_size > 0) {
			std::vector<char> cache_data(cache_size);
			res = vkGetPipelineCacheData(
					_device, vk_pipeline->vk_pipeline_cache, &cache_size, cache_data.data());

			if (res == VK_SUCCESS) {
				// Unified path with creation logic
				const auto tmp = std::filesystem::temp_directory_path();
				std::filesystem::path path =
						tmp / std::format("glitch/cache/{}.cache", vk_pipeline->shader_hash);

				if (!std::filesystem::exists(path.parent_path())) {
					std::error_code ec;
					std::filesystem::create_directories(path.parent_path(), ec);
					if (ec) {
						GL_LOG_ERROR("Failed to create cache directory: {}", ec.message());
					}
				}

				std::ofstream file(path, std::ios::binary);
				if (file) {
					PipelineCacheHeader header = {};
					header.magic_number = PIPELINE_CACHE_MAGIC_NUMBER;
					header.data_size = cache_size;
					header.vendor_id = _physical_device_properties.vendorID;
					header.device_id = _physical_device_properties.deviceID;
					header.driver_version = _physical_device_properties.driverVersion;
					memcpy(header.uuid, _physical_device_properties.pipelineCacheUUID,
							VK_UUID_SIZE * sizeof(char));

					file.write((const char*)&header, sizeof(PipelineCacheHeader));
					file.write(cache_data.data(), cache_size);
				} else {
					GL_LOG_ERROR("[VULKAN] Unable to write pipeline cache data to file!");
				}
			}
		}
	}

	vkDestroyPipeline(_device, vk_pipeline->vk_pipeline, nullptr);
	vkDestroyPipelineCache(_device, vk_pipeline->vk_pipeline_cache, nullptr);

	VersatileResource::free(_resources_allocator, pipeline);

	return {};
}

} //namespace gl
