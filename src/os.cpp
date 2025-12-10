#include "glgpu/os.h"

namespace gl {

WindowCompositor get_window_compositor() {
#if defined(_WIN32)
	return WindowingSystem::WIN32;
#elif defined(__linux__) || defined(__unix__)
	if (std::getenv("WAYLAND_DISPLAY")) {
		return WindowCompositor::WAYLAND;
	}

	// DISPLAY is always set in X sessions, but can be set for XWayland too.
	// However, if WAYLAND_DISPLAY is NOT set, DISPLAY usually means X11.
	if (std::getenv("DISPLAY")) {
		// You generally have to decide whether to use XCB or Xlib.
		// XCB is the modern, preferred X11 interface for Vulkan.
		// Assume XCB is available/preferred for a modern abstraction.
		return WindowCompositor::X11;
	}

	// XDG_SESSION_TYPE is often set to 'wayland', 'x11', or 'tty'
	const char* session_type = std::getenv("XDG_SESSION_TYPE");
	if (session_type) {
		if (strcmp(session_type, "wayland") == 0) {
			return WindowCompositor::WAYLAND;
		} else if (strcmp(session_type, "x11") == 0) {
			return WindowCompositor::X11; // Treat X11 as XCB for Vulkan
		}
	}

	return WindowCompositor::UNKNOWN;

#else
	return WindowingSystem::UNKNOWN;
#endif
}

} //namespace gl
