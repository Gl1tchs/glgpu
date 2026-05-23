#pragma once

#include "export.h"

#include <cstdint>
#include <format>
#include <string>

namespace gpukit {

enum LogLevel : uint8_t {
	LOG_LEVEL_TRACE = 0,
	LOG_LEVEL_INFO,
	LOG_LEVEL_WARNING,
	LOG_LEVEL_ERROR,
	LOG_LEVEL_FATAL,
};

class GPUKIT_API Logger {
public:
	static void log(LogLevel level, const std::string& fmt);
};

} //namespace gpukit

#define GPUKIT_LOG_TRACE(...) gpukit::Logger::log(gpukit::LOG_LEVEL_TRACE, std::format(__VA_ARGS__))
#define GPUKIT_LOG_INFO(...) gpukit::Logger::log(gpukit::LOG_LEVEL_INFO, std::format(__VA_ARGS__))
#define GPUKIT_LOG_WARNING(...) gpukit::Logger::log(gpukit::LOG_LEVEL_WARNING, std::format(__VA_ARGS__))
#define GPUKIT_LOG_ERROR(...) gpukit::Logger::log(gpukit::LOG_LEVEL_ERROR, std::format(__VA_ARGS__))
#define GPUKIT_LOG_FATAL(...) gpukit::Logger::log(gpukit::LOG_LEVEL_FATAL, std::format(__VA_ARGS__))
