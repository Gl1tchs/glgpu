#pragma once

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_WAYLAND
#endif

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "glgpu/device.h"
#include "glgpu/os.h"

namespace gl {

inline bool extract_glfw_info(DeviceCreateInfo& info, GLFWwindow* window) {
#if defined(_WIN32)
	info.native_window_handle = (void*)glfwGetWin32Window(window);
	info.native_connection_handle = nullptr;
	return true;
#elif defined(__linux__)
	if (get_window_compositor() == WindowCompositor::WAYLAND) {
		info.native_connection_handle = (void*)glfwGetWaylandDisplay();
		info.native_window_handle = (void*)glfwGetWaylandWindow(window);
		return true;
	} else {
		info.native_connection_handle = (void*)glfwGetX11Display();
		info.native_window_handle = (void*)(uintptr_t)glfwGetX11Window(window);
		return true;
	}
#else
	return false;
#endif
}

} //namespace gl
