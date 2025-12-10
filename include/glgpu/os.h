#pragma once

namespace gl {

enum class WindowCompositor {
	WIN32,
	WAYLAND,
	X11,
	UNKNOWN,
};

/**
 * Get the window compositor the user is on.
 */
WindowCompositor get_window_compositor();

} //namespace gl
