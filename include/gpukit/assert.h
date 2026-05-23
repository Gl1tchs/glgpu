#pragma once

#include "gpukit/log.h"

#if _WIN32
#define DEBUGBREAK() __debugbreak()
#elif __linux__
#include <signal.h>
#define DEBUGBREAK() raise(SIGTRAP)
#else
#error "Platform doesn't support debugbreak yet!"
#endif

#define GPUKIT_EXPAND_MACRO(x) x
#define GPUKIT_STRINGIFY_MACRO(x) #x

#define GPUKIT_INTERNAL_ASSERT_IMPL(check, msg, ...)                                                   \
	if (!(check)) {                                                                                \
		GPUKIT_LOG_FATAL(msg, __VA_ARGS__);                                                            \
		DEBUGBREAK();                                                                              \
	}

#define GPUKIT_INTERNAL_ASSERT_WITH_MSG(check, ...)                                                    \
	GPUKIT_INTERNAL_ASSERT_IMPL(check, "Assertion failed: {}", __VA_ARGS__)

#define GPUKIT_INTERNAL_ASSERT_NO_MSG(check)                                                           \
	GPUKIT_INTERNAL_ASSERT_IMPL(check, "Assertion '{}' failed at {}:{}", GPUKIT_STRINGIFY_MACRO(check),    \
			__FILE__, __LINE__)

#define GPUKIT_INTERNAL_ASSERT_GET_MACRO_NAME(arg1, arg2, macro, ...) macro

#define GPUKIT_INTERNAL_ASSERT_GET_MACRO(...)                                                          \
	GPUKIT_EXPAND_MACRO(GPUKIT_INTERNAL_ASSERT_GET_MACRO_NAME(                                             \
			__VA_ARGS__, GPUKIT_INTERNAL_ASSERT_WITH_MSG, GPUKIT_INTERNAL_ASSERT_NO_MSG))

#define GPUKIT_ASSERT(...) GPUKIT_EXPAND_MACRO(GPUKIT_INTERNAL_ASSERT_GET_MACRO(__VA_ARGS__)(__VA_ARGS__))
