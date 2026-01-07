#pragma once

#if defined(GLGPU_STATIC_BUILD)
#define GLGPU_API
#else
#if defined(_WIN32) || defined(__CYGWIN__)
#ifdef GLGPU_BUILD_DLL
#define GLGPU_API __declspec(dllexport)
#else
#define GLGPU_API __declspec(dllimport)
#endif
#else
#if __GNUC__ >= 4
#define GLGPU_API __attribute__((visibility("default")))
#else
#define GLGPU_API
#endif
#endif
#endif
