#pragma once

#include "glgpu/paged_allocator.h"

namespace gl {

template <typename... RESOURCE_TYPES> struct VersatileResourceTemplate {
	static constexpr size_t RESOURCE_SIZES[] = { sizeof(RESOURCE_TYPES)... };
	static constexpr size_t MAX_RESOURCE_SIZE =
			*std::max_element(RESOURCE_SIZES, RESOURCE_SIZES + sizeof...(RESOURCE_TYPES));
	uint8_t data[MAX_RESOURCE_SIZE];

	template <typename T> static T* allocate(PagedAllocator<VersatileResourceTemplate>& allocator) {
		VersatileResourceTemplate* obj = allocator.alloc();
		return new (obj->data) T;
	}

	template <typename T>
	static void free(PagedAllocator<VersatileResourceTemplate>& allocator, T* object) {
		allocator.free(reinterpret_cast<VersatileResourceTemplate*>(object));
	}
};

} //namespace gl
