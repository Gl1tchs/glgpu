#pragma once

#if defined(_WIN32)
#define GLFW_EXPOSE_NATIVE_WIN32
#elif defined(__APPLE__)
#define GLFW_EXPOSE_NATIVE_COCOA
#elif defined(__linux__)
#define GLFW_EXPOSE_NATIVE_X11
#define GLFW_EXPOSE_NATIVE_WAYLAND
#endif

#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>

#include "gpukit/device.h"
#include "gpukit/os.h"

#if defined(__APPLE__)
#include <objc/message.h>
#include <objc/runtime.h>
#endif

namespace gpukit {

#if defined(__APPLE__)
// Returns a CAMetalLayer* from an NSWindow* using the Obj-C runtime C API.
// Sets up wantsLayer and assigns a new CAMetalLayer to the content view.
inline void* _gpukit_metal_layer_from_nswindow(void* nswindow) {
	using msg_id_t = id (*)(id, SEL);
	using msg_set_bool_t = void (*)(id, SEL, BOOL);
	using msg_set_id_t = void (*)(id, SEL, id);
	using msg_cls_t = id (*)(Class, SEL);

	id ns_win = reinterpret_cast<id>(nswindow);
	id content_view =
			reinterpret_cast<msg_id_t>(objc_msgSend)(ns_win, sel_registerName("contentView"));
	reinterpret_cast<msg_set_bool_t>(objc_msgSend)(
			content_view, sel_registerName("setWantsLayer:"), YES);
	id metal_layer = reinterpret_cast<msg_cls_t>(objc_msgSend)(
			reinterpret_cast<Class>(objc_getClass("CAMetalLayer")), sel_registerName("layer"));
	reinterpret_cast<msg_set_id_t>(objc_msgSend)(
			content_view, sel_registerName("setLayer:"), metal_layer);
	return reinterpret_cast<void*>(metal_layer);
}
#endif

inline bool extract_glfw_info(DeviceCreateInfo& info, GLFWwindow* window) {
#if defined(_WIN32)
	info.native_window_handle = (void*)glfwGetWin32Window(window);
	info.native_connection_handle = nullptr;
	return true;
#elif defined(__APPLE__)
	void* ns_window = (void*)glfwGetCocoaWindow(window);
	info.native_connection_handle = nullptr;
	info.native_window_handle = _gpukit_metal_layer_from_nswindow(ns_window);
	return info.native_window_handle != nullptr;
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

} //namespace gpukit
