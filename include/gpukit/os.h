#pragma once

#include "gpukit/export.h"

namespace gpukit {

enum class WindowCompositor {
	WIN32,
	WAYLAND,
	X11,
	UNKNOWN,
};

/**
 * Get the window compositor the user is on.
 */
GPUKIT_API WindowCompositor get_window_compositor();

} //namespace gpukitkit
