#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

#include "gpukit/device.h"

namespace gpukit {

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

	// MacOS
#if defined(SDL_VIDEO_DRIVER_COCOA)
	if (wmInfo.subsystem == SDL_SYSWM_COCOA) {
		return false;
	}
#endif

	return false;
}

} //namespace gpukit
