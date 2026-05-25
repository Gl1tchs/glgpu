#include "gpukit/log.h"

#include <chrono>
#include <iostream>

namespace gpukit {

static constexpr const char* k_colors[] = {
	[static_cast<uint8_t>(LogLevel::Trace)]   = "\x1B[1m",
	[static_cast<uint8_t>(LogLevel::Info)]    = "\x1B[32m",
	[static_cast<uint8_t>(LogLevel::Warning)] = "\x1B[93m",
	[static_cast<uint8_t>(LogLevel::Error)]   = "\x1B[91m",
	[static_cast<uint8_t>(LogLevel::Fatal)]   = "\x1B[31m",
};

Logger& Logger::get() noexcept {
	static Logger instance;
	return instance;
}

void Logger::_write(LogLevel level, const std::string& msg) {
	const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
	std::clog << k_colors[static_cast<uint8_t>(level)]
	          << std::format("[{:%H:%M:%S}] {}", now, msg)
	          << "\x1B[0m\n";
}

} // namespace gpukit
