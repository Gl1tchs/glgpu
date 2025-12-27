#pragma once

#include "glgpu/assert.h"

#include <vk_mem_alloc.h>
#include <vulkan/vulkan_core.h>

// Asserting check (Use for internal logic that should never fail)
#define VK_CHECK(x)                                                                                \
	do {                                                                                           \
		VkResult err = x;                                                                          \
		if (err) {                                                                                 \
			GL_LOG_ERROR("[VULKAN] [VK_CHECK] {}", vk_result_to_string(err));                      \
			GL_ASSERT(false);                                                                      \
		}                                                                                          \
	} while (false)

// Returning check (Use for runtime operations that might fail, e.g., creation)
#define VK_CHECK_RET(x, error_code)                                                                \
	do {                                                                                           \
		VkResult err = x;                                                                          \
		if (err) {                                                                                 \
			GL_LOG_ERROR(                                                                          \
					"[VULKAN] Error: {} -> returning {}", vk_result_to_string(err), #error_code);  \
			return error_code;                                                                     \
		}                                                                                          \
	} while (false)

namespace gl {

inline const char* vk_result_to_string(VkResult res) {
#define CASE(x)                                                                                    \
	case VK_##x:                                                                                   \
		return #x;

	switch (res) {
		CASE(SUCCESS)
		CASE(NOT_READY)
		CASE(TIMEOUT)
		CASE(EVENT_SET)
		CASE(EVENT_RESET)
		CASE(INCOMPLETE)
		CASE(ERROR_OUT_OF_HOST_MEMORY)
		CASE(ERROR_OUT_OF_DEVICE_MEMORY)
		CASE(ERROR_INITIALIZATION_FAILED)
		CASE(ERROR_DEVICE_LOST)
		CASE(ERROR_MEMORY_MAP_FAILED)
		CASE(ERROR_LAYER_NOT_PRESENT)
		CASE(ERROR_EXTENSION_NOT_PRESENT)
		CASE(ERROR_FEATURE_NOT_PRESENT)
		CASE(ERROR_INCOMPATIBLE_DRIVER)
		CASE(ERROR_TOO_MANY_OBJECTS)
		CASE(ERROR_FORMAT_NOT_SUPPORTED)
		CASE(ERROR_FRAGMENTED_POOL)
		CASE(ERROR_UNKNOWN)
		CASE(ERROR_OUT_OF_POOL_MEMORY)
		CASE(ERROR_INVALID_EXTERNAL_HANDLE)
		CASE(ERROR_FRAGMENTATION)
		CASE(ERROR_INVALID_OPAQUE_CAPTURE_ADDRESS)
		CASE(PIPELINE_COMPILE_REQUIRED)
		CASE(ERROR_SURFACE_LOST_KHR)
		CASE(ERROR_NATIVE_WINDOW_IN_USE_KHR)
		CASE(SUBOPTIMAL_KHR)
		CASE(ERROR_OUT_OF_DATE_KHR)
		CASE(ERROR_INCOMPATIBLE_DISPLAY_KHR)
		CASE(ERROR_VALIDATION_FAILED_EXT)
		CASE(ERROR_INVALID_SHADER_NV)
		default:
			return "UNKNOWN_VK_RESULT";
	}
#undef CASE
}

} // namespace gl
