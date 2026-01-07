#pragma once

#include "glgpu/export.h"

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
GLGPU_API WindowCompositor get_window_compositor();

} //namespace gl
