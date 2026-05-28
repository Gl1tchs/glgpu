#pragma once

#include "export.h"

#include <cstdint>
#include <string>

#if defined(GPUKIT_USE_FMT_FORMAT)
#define FMT_HEADER_ONLY
#include <fmt/format.h>
#include <fmt/chrono.h>
#define GPUKIT_FMT_STRING(...) fmt::format_string<__VA_ARGS__>
#define GPUKIT_FMT_FORMAT(f, ...) fmt::format(f, ##__VA_ARGS__)
#else
#include <format>
#define GPUKIT_FMT_STRING(...) std::format_string<__VA_ARGS__>
#define GPUKIT_FMT_FORMAT(f, ...) std::format(f, ##__VA_ARGS__)
#endif

namespace gpukit {

enum class LogLevel : uint8_t { Trace, Info, Warning, Error, Fatal };

namespace log {

GPUKIT_API void _write(LogLevel level, const std::string& msg);

template <typename... Args>
void trace(GPUKIT_FMT_STRING(Args...) fmt, Args&&... args) {
	_write(LogLevel::Trace, GPUKIT_FMT_FORMAT(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void info(GPUKIT_FMT_STRING(Args...) fmt, Args&&... args) {
	_write(LogLevel::Info, GPUKIT_FMT_FORMAT(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void warn(GPUKIT_FMT_STRING(Args...) fmt, Args&&... args) {
	_write(LogLevel::Warning, GPUKIT_FMT_FORMAT(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void error(GPUKIT_FMT_STRING(Args...) fmt, Args&&... args) {
	_write(LogLevel::Error, GPUKIT_FMT_FORMAT(fmt, std::forward<Args>(args)...));
}

template <typename... Args>
void fatal(GPUKIT_FMT_STRING(Args...) fmt, Args&&... args) {
	_write(LogLevel::Fatal, GPUKIT_FMT_FORMAT(fmt, std::forward<Args>(args)...));
}

} // namespace log

} // namespace gpukit

#define GPUKIT_LOG_TRACE(...)   gpukit::log::trace(__VA_ARGS__)
#define GPUKIT_LOG_INFO(...)    gpukit::log::info(__VA_ARGS__)
#define GPUKIT_LOG_WARNING(...) gpukit::log::warn(__VA_ARGS__)
#define GPUKIT_LOG_ERROR(...)   gpukit::log::error(__VA_ARGS__)
#define GPUKIT_LOG_FATAL(...)   gpukit::log::fatal(__VA_ARGS__)
