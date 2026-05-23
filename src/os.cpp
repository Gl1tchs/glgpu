#include "gpukit/os.h"

#include <cstdlib>
#include <cstring>

namespace gpukit {

static std::optional<WindowCompositor> s_overriden_compositor = std::nullopt;

WindowCompositor get_window_compositor() {
	if (s_overriden_compositor.has_value()) {
		return s_overriden_compositor.value();
	}

#if defined(_WIN32)
	return WindowCompositor::WIN32;
#elif defined(__APPLE__)
	return WindowCompositor::MACOS;
#elif defined(__linux__) || defined(__unix__) || defined(__FreeBSD__)
	if (std::getenv("WAYLAND_DISPLAY")) {
		return WindowCompositor::WAYLAND;
	}
	if (std::getenv("DISPLAY")) {
		return WindowCompositor::X11;
	}

	if (const char* session_type = std::getenv("XDG_SESSION_TYPE")) {
		if (std::strcmp(session_type, "wayland") == 0) {
			return WindowCompositor::WAYLAND;
		}
		if (std::strcmp(session_type, "x11") == 0) {
			return WindowCompositor::X11;
		}
	}

	return WindowCompositor::UNKNOWN;

#else
	return WindowCompositor::UNKNOWN;
#endif
}

void override_window_compositor(std::optional<WindowCompositor> compositor) {
	s_overriden_compositor = compositor;
}

} //namespace gpukit
