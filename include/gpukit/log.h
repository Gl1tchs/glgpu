#pragma once

#include "export.h"

#include <cstdint>
#include <format>
#include <string>

namespace gpukit {

enum class LogLevel : uint8_t { Trace, Info, Warning, Error, Fatal };

class GPUKIT_API Logger {
public:
	static Logger& get() noexcept;

	template <typename... Args>
	void log(LogLevel level, std::format_string<Args...> fmt, Args&&... args) {
		_write(level, std::format(fmt, std::forward<Args>(args)...));
	}

	template <typename... Args>
	void trace(std::format_string<Args...> fmt, Args&&... args) {
		log(LogLevel::Trace, fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void info(std::format_string<Args...> fmt, Args&&... args) {
		log(LogLevel::Info, fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void warn(std::format_string<Args...> fmt, Args&&... args) {
		log(LogLevel::Warning, fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void error(std::format_string<Args...> fmt, Args&&... args) {
		log(LogLevel::Error, fmt, std::forward<Args>(args)...);
	}

	template <typename... Args>
	void fatal(std::format_string<Args...> fmt, Args&&... args) {
		log(LogLevel::Fatal, fmt, std::forward<Args>(args)...);
	}

private:
	void _write(LogLevel level, const std::string& msg);
};

} // namespace gpukit

#define GPUKIT_LOG_TRACE(...)   gpukit::Logger::get().trace(__VA_ARGS__)
#define GPUKIT_LOG_INFO(...)    gpukit::Logger::get().info(__VA_ARGS__)
#define GPUKIT_LOG_WARNING(...) gpukit::Logger::get().warn(__VA_ARGS__)
#define GPUKIT_LOG_ERROR(...)   gpukit::Logger::get().error(__VA_ARGS__)
#define GPUKIT_LOG_FATAL(...)   gpukit::Logger::get().fatal(__VA_ARGS__)
