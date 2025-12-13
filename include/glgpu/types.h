#pragma once

#include "glgpu/color.h"
#include "glgpu/vec.h"

namespace gl {

// -----------------------------------------------------------------------------
// Handles & Macros
// -----------------------------------------------------------------------------

// Defines handles that are not needed to be freed by user
#define GL_DEFINE_HANDLE(object) typedef struct object##_T* object;

// Defines handles that are needed to be freed by user
#define GL_DEFINE_NON_DISPATCHABLE_HANDLE(object) typedef struct object##_T* object;

GL_DEFINE_NON_DISPATCHABLE_HANDLE(Buffer)
GL_DEFINE_NON_DISPATCHABLE_HANDLE(Image)
GL_DEFINE_NON_DISPATCHABLE_HANDLE(Sampler)
GL_DEFINE_NON_DISPATCHABLE_HANDLE(CommandPool)
GL_DEFINE_HANDLE(CommandBuffer)
GL_DEFINE_NON_DISPATCHABLE_HANDLE(CommandQueue)
GL_DEFINE_NON_DISPATCHABLE_HANDLE(RenderPass)
GL_DEFINE_NON_DISPATCHABLE_HANDLE(FrameBuffer)
GL_DEFINE_NON_DISPATCHABLE_HANDLE(Swapchain)
GL_DEFINE_NON_DISPATCHABLE_HANDLE(Pipeline)
GL_DEFINE_NON_DISPATCHABLE_HANDLE(Shader)
GL_DEFINE_NON_DISPATCHABLE_HANDLE(UniformSet)
GL_DEFINE_NON_DISPATCHABLE_HANDLE(Fence)
GL_DEFINE_NON_DISPATCHABLE_HANDLE(Semaphore)

#define GL_NULL_HANDLE nullptr
#define GL_REMAINING_MIP_LEVELS (~0U)
#define GL_REMAINING_ARRAY_LAYERS (~0U)

// -----------------------------------------------------------------------------
// Common Enums & Errors
// -----------------------------------------------------------------------------

/**
 * Global Error Enumeration
 * Combines specific errors (Swapchain, Surface, Generic) into one type.
 */
enum class Error {
	NONE = 0,
	// Generic
	UNKNOWN,
	OUT_OF_MEMORY,
	DEVICE_LOST,
	// Surface / Windowing
	SURFACE_INVALID_COMPOSITOR,
	SURFACE_SWAPCHAIN_NOT_SUPPORTED,
	// Swapchain
	SWAPCHAIN_OUT_OF_DATE, // Resize needed
	SWAPCHAIN_LOST,
	// Validation
	VALIDATION_FAILED
};

enum class MemoryAllocationType {
	CPU,
	GPU,
};

enum class RenderAPI {
	VULKAN,
};

// -----------------------------------------------------------------------------
// Data Formats
// -----------------------------------------------------------------------------

enum class DataFormat : int {
	UNDEFINED = 0,
	R8_UNORM = 9,
	R8_SNORM = 10,
	R8_USCALED = 11,
	R8_SSCALED = 12,
	R8_UINT = 13,
	R8_SINT = 14,
	R8_SRGB = 15,
	R8G8_UNORM = 16,
	R8G8_SNORM = 17,
	R8G8_USCALED = 18,
	R8G8_SSCALED = 19,
	R8G8_UINT = 20,
	R8G8_SINT = 21,
	R8G8_SRGB = 22,
	R8G8B8_UNORM = 23,
	R8G8B8_SNORM = 24,
	R8G8B8_USCALED = 25,
	R8G8B8_SSCALED = 26,
	R8G8B8_UINT = 27,
	R8G8B8_SINT = 28,
	R8G8B8_SRGB = 29,
	B8G8R8_UNORM = 30,
	B8G8R8_SNORM = 31,
	B8G8R8_USCALED = 32,
	B8G8R8_SSCALED = 33,
	B8G8R8_UINT = 34,
	B8G8R8_SINT = 35,
	B8G8R8_SRGB = 36,
	R8G8B8A8_UNORM = 37,
	R8G8B8A8_SNORM = 38,
	R8G8B8A8_USCALED = 39,
	R8G8B8A8_SSCALED = 40,
	R8G8B8A8_UINT = 41,
	R8G8B8A8_SINT = 42,
	R8G8B8A8_SRGB = 43,
	B8G8R8A8_UNORM = 44,
	B8G8R8A8_SNORM = 45,
	B8G8R8A8_USCALED = 46,
	B8G8R8A8_SSCALED = 47,
	B8G8R8A8_UINT = 48,
	B8G8R8A8_SINT = 49,
	B8G8R8A8_SRGB = 50,
	A8B8G8R8_UNORM_PACK32 = 51,
	A8B8G8R8_SNORM_PACK32 = 52,
	A8B8G8R8_USCALED_PACK32 = 53,
	A8B8G8R8_SSCALED_PACK32 = 54,
	A8B8G8R8_UINT_PACK32 = 55,
	A8B8G8R8_SINT_PACK32 = 56,
	A8B8G8R8_SRGB_PACK32 = 57,
	R16_UNORM = 70,
	R16_SNORM = 71,
	R16_USCALED = 72,
	R16_SSCALED = 73,
	R16_UINT = 74,
	R16_SINT = 75,
	R16_SFLOAT = 76,
	R16G16_UNORM = 77,
	R16G16_SNORM = 78,
	R16G16_USCALED = 79,
	R16G16_SSCALED = 80,
	R16G16_UINT = 81,
	R16G16_SINT = 82,
	R16G16_SFLOAT = 83,
	R16G16B16_UNORM = 84,
	R16G16B16_SNORM = 85,
	R16G16B16_USCALED = 86,
	R16G16B16_SSCALED = 87,
	R16G16B16_UINT = 88,
	R16G16B16_SINT = 89,
	R16G16B16_SFLOAT = 90,
	R16G16B16A16_UNORM = 91,
	R16G16B16A16_SNORM = 92,
	R16G16B16A16_USCALED = 93,
	R16G16B16A16_SSCALED = 94,
	R16G16B16A16_UINT = 95,
	R16G16B16A16_SINT = 96,
	R16G16B16A16_SFLOAT = 97,
	R32_UINT = 98,
	R32_SINT = 99,
	R32_SFLOAT = 100,
	R32G32_UINT = 101,
	R32G32_SINT = 102,
	R32G32_SFLOAT = 103,
	R32G32B32_UINT = 104,
	R32G32B32_SINT = 105,
	R32G32B32_SFLOAT = 106,
	R32G32B32A32_UINT = 107,
	R32G32B32A32_SINT = 108,
	R32G32B32A32_SFLOAT = 109,
	D16_UNORM = 124,
	D16_UNORM_S8_UINT = 128,
	D24_UNORM_S8_UINT = 129,
	D32_SFLOAT = 126,
	MAX = 0x7FFFFFFF,
};

size_t get_data_format_size(DataFormat p_format);
bool is_depth_format(DataFormat p_format);

// -----------------------------------------------------------------------------
// Buffers
// -----------------------------------------------------------------------------

enum BufferUsageBits : uint32_t {
	BUFFER_USAGE_TRANSFER_SRC_BIT = 0x00000001,
	BUFFER_USAGE_TRANSFER_DST_BIT = 0x00000002,
	BUFFER_USAGE_UNIFORM_TEXEL_BUFFER_BIT = 0x00000004,
	BUFFER_USAGE_STORAGE_TEXEL_BUFFER_BIT = 0x00000008,
	BUFFER_USAGE_UNIFORM_BUFFER_BIT = 0x00000010,
	BUFFER_USAGE_STORAGE_BUFFER_BIT = 0x00000020,
	BUFFER_USAGE_INDEX_BUFFER_BIT = 0x00000040,
	BUFFER_USAGE_VERTEX_BUFFER_BIT = 0x00000080,
	BUFFER_USAGE_INDIRECT_BUFFER_BIT = 0x00000100,
	BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT = 0x00020000,
};
typedef uint32_t BufferUsageFlags;

typedef uint64_t BufferDeviceAddress;

struct BufferCopyRegion {
	uint64_t src_offset;
	uint64_t dst_offset;
	uint64_t size;
};

// -----------------------------------------------------------------------------
// Images & Samplers
// -----------------------------------------------------------------------------

enum class ImageLayout : uint32_t {
	UNDEFINED = 0,
	GENERAL = 1,
	COLOR_ATTACHMENT_OPTIMAL = 2,
	DEPTH_STENCIL_ATTACHMENT_OPTIMAL = 3,
	DEPTH_STENCIL_READ_ONLY_OPTIMAL = 4,
	SHADER_READ_ONLY_OPTIMAL = 5,
	TRANSFER_SRC_OPTIMAL = 6,
	TRANSFER_DST_OPTIMAL = 7,
	PRESENT_SRC = 1000001002,
};

enum class ImageFiltering { NEAREST, LINEAR };

enum class ImageWrappingMode {
	REPEAT,
	MIRRORED_REPEAT,
	CLAMP_TO_EDGE,
	CLAMP_TO_BORDER,
	MIRROR_CLAMP_TO_EDGE,
};

enum ImageAspectBits : uint32_t {
	IMAGE_ASPECT_COLOR_BIT = 0x00000001,
	IMAGE_ASPECT_DEPTH_BIT = 0x00000002,
	IMAGE_ASPECT_STENCIL_BIT = 0x00000004,
};
typedef uint32_t ImageAspectFlags;

enum ImageUsageBits : uint32_t {
	IMAGE_USAGE_TRANSFER_SRC_BIT = 0x00000001,
	IMAGE_USAGE_TRANSFER_DST_BIT = 0x00000002,
	IMAGE_USAGE_SAMPLED_BIT = 0x00000004,
	IMAGE_USAGE_STORAGE_BIT = 0x00000008,
	IMAGE_USAGE_COLOR_ATTACHMENT_BIT = 0x00000010,
	IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT = 0x00000020,
};
typedef uint32_t ImageUsageFlags;

struct ImageSubresourceLayers {
	ImageAspectFlags aspect_mask;
	uint32_t mip_level;
	uint32_t base_array_layer;
	uint32_t layer_count;
};

struct BufferImageCopyRegion {
	uint64_t buffer_offset;
	uint32_t buffer_row_length;
	uint32_t buffer_image_height;
	ImageSubresourceLayers image_subresource;
	Vec3u image_offset;
	Vec3u image_extent;
};

struct ImageCreateInfo {
	DataFormat format = DataFormat::UNDEFINED;
	Vec2u size = { 0, 0 };
	const void* data = nullptr; // Optional initial data
	ImageUsageFlags usage = IMAGE_USAGE_SAMPLED_BIT;
	bool mipmapped = false;
	uint32_t samples = 1;
};

struct SamplerCreateInfo {
	ImageFiltering min_filter = ImageFiltering::LINEAR;
	ImageFiltering mag_filter = ImageFiltering::LINEAR;
	ImageWrappingMode wrap_u = ImageWrappingMode::CLAMP_TO_EDGE;
	ImageWrappingMode wrap_v = ImageWrappingMode::CLAMP_TO_EDGE;
	ImageWrappingMode wrap_w = ImageWrappingMode::CLAMP_TO_EDGE;
	uint32_t mip_levels = 0;
};

// -----------------------------------------------------------------------------
// Shaders & Uniforms
// -----------------------------------------------------------------------------

enum ShaderStageBits : uint32_t {
	SHADER_STAGE_VERTEX_BIT = 0x00000001,
	SHADER_STAGE_TESSELLATION_CONTROL_BIT = 0x00000002,
	SHADER_STAGE_TESSELLATION_EVALUATION_BIT = 0x00000004,
	SHADER_STAGE_GEOMETRY_BIT = 0x00000008,
	SHADER_STAGE_FRAGMENT_BIT = 0x00000010,
	SHADER_STAGE_COMPUTE_BIT = 0x00000020,
};
typedef uint32_t ShaderStageFlags;

struct SpirvEntry {
	std::vector<uint32_t> byte_code;
	ShaderStageFlags stage;
};

struct ShaderInterfaceVariable {
	const char* name;
	uint32_t location;
	DataFormat format;
};

constexpr uint32_t MAX_UNIFORM_SETS = 16;

enum class ShaderUniformType : uint32_t {
	SAMPLER,
	SAMPLER_WITH_TEXTURE,
	TEXTURE,
	IMAGE,
	UNIFORM_BUFFER,
	STORAGE_BUFFER,
	MAX,
};

struct ShaderUniform {
	ShaderUniformType type = ShaderUniformType::MAX;
	uint32_t binding = 0xffffffff;
	std::vector<void*> data;
};

// -----------------------------------------------------------------------------
// Pipeline States
// -----------------------------------------------------------------------------

enum class CompareOperator {
	NEVER,
	LESS,
	EQUAL,
	LESS_OR_EQUAL,
	GREATER,
	NOT_EQUAL,
	GREATER_OR_EQUAL,
	ALWAYS
};

enum class RenderPrimitive : uint32_t {
	POINT_LIST,
	LINE_LIST,
	LINE_STRIP,
	TRIANGLE_LIST,
	TRIANGLE_STRIP,
	TRIANGLE_FAN,
	LINE_LIST_WITH_ADJACENCY,
	LINE_STRIP_WITH_ADJACENCY,
	TRIANGLE_LIST_WITH_ADJACENCY,
	TRIANGLE_STRIP_WITH_ADJACENCY,
	PATCH_LIST,
};

enum class PolygonCullMode : uint32_t { DISABLED, FRONT, BACK };
enum class PolygonFrontFace : uint32_t { CLOCKWISE, COUNTER_CLOCKWISE };

enum class StencilOperator : uint32_t {
	KEEP,
	ZERO,
	REPLACE,
	INCREMENT_AND_CLAMP,
	DECREMENT_AND_CLAMP,
	INVERT,
	INCREMENT_AND_WRAP,
	DECREMENT_AND_WRAP,
};

enum class LogicOperator : uint32_t {
	CLEAR,
	AND,
	AND_REVERSE,
	COPY,
	AND_INVERTED,
	NO_OP,
	XOR,
	OR,
	NOR,
	EQUIVALENT,
	INVERT,
	OR_REVERSE,
	COPY_INVERTED,
	OR_INVERTED,
	NAND,
	SET,
};

enum class BlendFactor : uint32_t {
	ZERO,
	ONE,
	SRC_COLOR,
	ONE_MINUS_SRC_COLOR,
	DST_COLOR,
	ONE_MINUS_DST_COLOR,
	SRC_ALPHA,
	ONE_MINUS_SRC_ALPHA,
	DST_ALPHA,
	ONE_MINUS_DST_ALPHA,
	CONSTANT_COLOR,
	ONE_MINUS_CONSTANT_COLOR,
	CONSTANT_ALPHA,
	ONE_MINUS_CONSTANT_ALPHA,
	SRC_ALPHA_SATURATE,
	SRC1_COLOR,
	ONE_MINUS_SRC1_COLOR,
	SRC1_ALPHA,
	ONE_MINUS_SRC1_ALPHA,
};

enum class BlendOperation : uint32_t { ADD, SUBTRACT, REVERSE_SUBTRACT, MIN, MAX };

enum PipelineDynamicStateBits {
	PIPELINE_DYNAMIC_STATE_LINE_WIDTH = 0x00000001,
	PIPELINE_DYNAMIC_STATE_DEPTH_BIAS = 0x00000002,
	PIPELINE_DYNAMIC_STATE_BLEND_CONSTANTS = 0x00000004,
	PIPELINE_DYNAMIC_STATE_DEPTH_BOUNDS = 0x00000008,
	PIPELINE_DYNAMIC_STATE_STENCIL_COMPARE_MASK = 0x00000010,
	PIPELINE_DYNAMIC_STATE_STENCIL_WRITE_MASK = 0x00000020,
	PIPELINE_DYNAMIC_STATE_STENCIL_REFERENCE = 0x00000040,
};
typedef uint32_t PipelineDynamicStateFlags;

struct PipelineVertexInputState {
	uint32_t stride = 0;
};

struct PipelineRasterizationState {
	bool enable_depth_clamp = false;
	bool discard_primitives = false;
	bool wireframe = false;
	PolygonCullMode cull_mode = PolygonCullMode::DISABLED;
	PolygonFrontFace front_face = PolygonFrontFace::CLOCKWISE;
	bool depth_bias_enabled = false;
	float depth_bias_constant_factor = 0.0f;
	float depth_bias_clamp = 0.0f;
	float depth_bias_slope_factor = 0.0f;
	float line_width = 1.0f;
};

struct PipelineMultisampleState {
	uint32_t sample_count = 1;
	bool enable_sample_shading = false;
	float min_sample_shading = 0.0f;
	std::vector<uint32_t> sample_mask;
	bool enable_alpha_to_coverage = false;
	bool enable_alpha_to_one = false;
};

struct PipelineDepthStencilState {
	bool enable_depth_test = false;
	bool enable_depth_write = false;
	CompareOperator depth_compare_operator = CompareOperator::ALWAYS;
	bool enable_depth_range = false;
	float depth_range_min = 0;
	float depth_range_max = 1.0f;
	bool enable_stencil = false;

	struct StencilOperationState {
		StencilOperator fail = StencilOperator::ZERO;
		StencilOperator pass = StencilOperator::ZERO;
		StencilOperator depth_fail = StencilOperator::ZERO;
		CompareOperator compare = CompareOperator::ALWAYS;
		uint32_t compare_mask = 0;
		uint32_t write_mask = 0;
		uint32_t reference = 0;
	};

	StencilOperationState front_op;
	StencilOperationState back_op;
};

struct PipelineColorBlendState {
	bool enable_logic_op = false;
	LogicOperator logic_op = LogicOperator::CLEAR;

	struct Attachment {
		bool enable_blend = false;
		BlendFactor src_color_blend_factor = BlendFactor::ZERO;
		BlendFactor dst_color_blend_factor = BlendFactor::ZERO;
		BlendOperation color_blend_op = BlendOperation::ADD;
		BlendFactor src_alpha_blend_factor = BlendFactor::ZERO;
		BlendFactor dst_alpha_blend_factor = BlendFactor::ZERO;
		BlendOperation alpha_blend_op = BlendOperation::ADD;
		bool write_r = true;
		bool write_g = true;
		bool write_b = true;
		bool write_a = true;
	};

	static PipelineColorBlendState create_disabled(int p_attachments = 1) {
		PipelineColorBlendState bs;
		for (int i = 0; i < p_attachments; i++)
			bs.attachments.push_back(Attachment());
		return bs;
	}

	static PipelineColorBlendState create_blend(int p_attachments = 1) {
		PipelineColorBlendState bs;
		for (int i = 0; i < p_attachments; i++) {
			Attachment ba;
			ba.enable_blend = true;
			ba.src_color_blend_factor = BlendFactor::SRC_ALPHA;
			ba.dst_color_blend_factor = BlendFactor::ONE_MINUS_SRC_ALPHA;
			ba.src_alpha_blend_factor = BlendFactor::SRC_ALPHA;
			ba.dst_alpha_blend_factor = BlendFactor::ONE_MINUS_SRC_ALPHA;
			bs.attachments.push_back(ba);
		}
		return bs;
	}

	std::vector<Attachment> attachments;
	Vec4f blend_constant;
};

struct PipelineRenderingState {
	std::vector<DataFormat> color_attachments;
	DataFormat depth_attachment = DataFormat::UNDEFINED;
};

struct RenderPipelineCreateInfo {
	Shader shader = GL_NULL_HANDLE;
	RenderPrimitive primitive = RenderPrimitive::TRIANGLE_LIST;

	// State Descriptors
	PipelineVertexInputState vertex_input_state;
	PipelineRasterizationState rasterization_state;
	PipelineMultisampleState multisample_state;
	PipelineDepthStencilState depth_stencil_state;
	PipelineColorBlendState color_blend_state;
	PipelineDynamicStateFlags dynamic_state = 0;

	// Target Definition (Legacy RenderPass OR Dynamic Rendering)
	RenderPass render_pass = GL_NULL_HANDLE;
	PipelineRenderingState rendering_info; // Used if render_pass is NULL
};

enum class PipelineType { GRAPHICS, COMPUTE };

// -----------------------------------------------------------------------------
// Render Pass & Attachments
// -----------------------------------------------------------------------------

enum class AttachmentLoadOp : uint32_t {
	LOAD = 0,
	CLEAR = 1,
	DONT_CARE = 2,
	NONE = 1000400000,
};

enum class AttachmentStoreOp : uint32_t {
	STORE = 0,
	DONT_CARE = 1,
	NONE = 1000301000,
};

struct RenderPassAttachment {
	DataFormat format;
	AttachmentLoadOp load_op = AttachmentLoadOp::CLEAR;
	AttachmentStoreOp store_op = AttachmentStoreOp::STORE;
	ImageLayout final_layout = ImageLayout::UNDEFINED;
	uint32_t sample_count = 1;
	bool is_depth_attachment = false;
};

enum SubpassAttachmentType {
	SUBPASS_ATTACHMENT_COLOR,
	SUBPASS_ATTACHMENT_DEPTH_STENCIL,
	SUBPASS_ATTACHMENT_INPUT,
};

struct SubpassAttachment {
	uint32_t attachment_index;
	SubpassAttachmentType type;
};

struct SubpassInfo {
	std::vector<SubpassAttachment> attachments;
};

enum ResolveModeBits : uint32_t {
	RESOLVE_MODE_NONE = 0,
	RESOLVE_MODE_SAMPLE_ZERO_BIT = 0x00000001,
	RESOLVE_MODE_AVERAGE_BIT = 0x00000002,
	RESOLVE_MODE_MIN_BIT = 0x00000004,
	RESOLVE_MODE_MAX_BIT = 0x00000008,
};
typedef uint32_t ResolveModeFlags;

struct ImageResolve {
	ImageSubresourceLayers src_subresource;
	Vec3i src_offset;
	ImageSubresourceLayers dst_subresource;
	Vec3i dst_offset;
	Vec3u extent;
};

struct RenderingAttachment {
	Image image;
	ImageLayout layout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
	AttachmentLoadOp load_op = AttachmentLoadOp::DONT_CARE;
	AttachmentStoreOp store_op = AttachmentStoreOp::STORE;
	Color clear_color = COLOR_BLACK;

	// For MSAA
	ResolveModeFlags resolve_mode = RESOLVE_MODE_NONE;
	Image resolve_image = GL_NULL_HANDLE;
	ImageLayout resolve_layout = ImageLayout::COLOR_ATTACHMENT_OPTIMAL;
};

// -----------------------------------------------------------------------------
// Queues & Commands
// -----------------------------------------------------------------------------

enum class QueueType { GRAPHICS, PRESENT, TRANSFER, COMPUTE };
enum class IndexType : uint32_t { UINT16 = 1, UINT32 = 2 };

} // namespace gl
