#pragma once

#include <android_native_app_glue.h>

#include "gpukit/device.h"
#include "gpukit/os.h"

namespace gpukit {

/**
 * Populate DeviceCreateInfo with the ANativeWindow* from an android_app.
 * Call this only after APP_CMD_INIT_WINDOW (i.e. app->window != nullptr).
 * Returns false if the window is not yet available.
 */
inline bool extract_android_info(DeviceCreateInfo& info, android_app* app) {
    if (!app || !app->window) {
        return false;
    }
    override_window_compositor(WindowCompositor::ANDROID_SURFACE);
    info.native_connection_handle = nullptr; // unused on Android
    info.native_window_handle = app->window; // ANativeWindow*
    return true;
}

} // namespace gpukit
