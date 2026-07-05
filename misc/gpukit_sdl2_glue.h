#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

#include "gpukit/device.h"
#include "gpukit/os.h"

#if defined(__APPLE__)
#include <objc/message.h>
#include <objc/runtime.h>
#endif

namespace gpukit {

#if defined(__APPLE__)
// Returns a CAMetalLayer* from an NSWindow* using the Obj-C runtime C API.
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

inline bool extract_sdl2_info(DeviceCreateInfo& info, SDL_Window* window) {
	SDL_SysWMinfo wmInfo;
	SDL_VERSION(&wmInfo.version);

	if (!SDL_GetWindowWMInfo(window, &wmInfo)) {
		return false;
	}

#if defined(SDL_VIDEO_DRIVER_WINDOWS)
	if (wmInfo.subsystem == SDL_SYSWM_WINDOWS) {
		info.native_window_handle = (void*)wmInfo.info.win.window;
		info.native_connection_handle = (void*)wmInfo.info.win.hinstance;
		return true;
	}
#endif

#if defined(SDL_VIDEO_DRIVER_X11)
	if (wmInfo.subsystem == SDL_SYSWM_X11) {
		override_window_compositor(WindowCompositor::X11);
		info.native_window_handle = (void*)wmInfo.info.x11.window;
		info.native_connection_handle = (void*)wmInfo.info.x11.display;
		return true;
	}
#endif

#if defined(SDL_VIDEO_DRIVER_WAYLAND)
	if (wmInfo.subsystem == SDL_SYSWM_WAYLAND) {
		override_window_compositor(WindowCompositor::WAYLAND);
		info.native_connection_handle = (void*)wmInfo.info.wl.display;
		info.native_window_handle = (void*)wmInfo.info.wl.surface;
		return true;
	}
#endif

#if defined(SDL_VIDEO_DRIVER_COCOA)
	if (wmInfo.subsystem == SDL_SYSWM_COCOA) {
		info.native_connection_handle = nullptr;
		info.native_window_handle =
				_gpukit_metal_layer_from_nswindow((void*)wmInfo.info.cocoa.window);
		return info.native_window_handle != nullptr;
	}
#endif

	return false;
}

} //namespace gpukit
