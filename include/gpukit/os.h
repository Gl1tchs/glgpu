#pragma once

#include "gpukit/export.h"

#include <optional>

namespace gpukit {

enum class WindowCompositor {
	WIN32,
	WAYLAND,
	X11,
	MACOS,
	ANDROID_SURFACE,
	UNKNOWN,
};

/**
 * Get the window compositor the user is on.
 */
GPUKIT_API WindowCompositor get_window_compositor();

/**
 * Override window compositor. Required for XWayland sessions.
 */
GPUKIT_API void override_window_compositor(std::optional<WindowCompositor> compositor);

} //namespace gpukit
