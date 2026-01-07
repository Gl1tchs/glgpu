#pragma once

#include <cstdint>
#include <format>

namespace gl {

enum LogLevel : uint8_t {
	LOG_LEVEL_TRACE = 0,
	LOG_LEVEL_INFO,
	LOG_LEVEL_WARNING,
	LOG_LEVEL_ERROR,
	LOG_LEVEL_FATAL,
};

class Logger {
public:
	static void log(LogLevel level, const std::string& fmt);
};

} //namespace gl

#define GL_LOG_TRACE(...) gl::Logger::log(gl::LOG_LEVEL_TRACE, std::format(__VA_ARGS__))
#define GL_LOG_INFO(...) gl::Logger::log(gl::LOG_LEVEL_INFO, std::format(__VA_ARGS__))
#define GL_LOG_WARNING(...) gl::Logger::log(gl::LOG_LEVEL_WARNING, std::format(__VA_ARGS__))
#define GL_LOG_ERROR(...) gl::Logger::log(gl::LOG_LEVEL_ERROR, std::format(__VA_ARGS__))
#define GL_LOG_FATAL(...) gl::Logger::log(gl::LOG_LEVEL_FATAL, std::format(__VA_ARGS__))
