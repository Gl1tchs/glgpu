#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>

#include "glgpu/device.h"

namespace gl {

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
		info.native_window_handle = (void*)wmInfo.info.x11.window;
		info.native_connection_handle = (void*)wmInfo.info.x11.display;
		return true;
	}
#endif

#if defined(SDL_VIDEO_DRIVER_WAYLAND)
	if (wmInfo.subsystem == SDL_SYSWM_WAYLAND) {
		return false;
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

} //namespace gl
