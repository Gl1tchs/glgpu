#pragma once

#if defined(GPUKIT_STATIC_BUILD)
#define GPUKIT_API
#else
#if defined(_WIN32) || defined(__CYGWIN__)
#ifdef GPUKIT_BUILD_DLL
#define GPUKIT_API __declspec(dllexport)
#else
#define GPUKIT_API __declspec(dllimport)
#endif
#else
#if __GNUC__ >= 4
#define GPUKIT_API __attribute__((visibility("default")))
#else
#define GPUKIT_API
#endif
#endif
#endif
