#include "gpukit/log.h"

#if defined(__ANDROID__)
#include <android/log.h>
#else
#include <chrono>
#include <iostream>
#endif

namespace gpukit::log {

void _write(LogLevel level, const std::string& msg) {
#if defined(__ANDROID__)
	int prio;
	switch (level) {
		case LogLevel::Trace:
			prio = ANDROID_LOG_VERBOSE;
			break;
		case LogLevel::Info:
			prio = ANDROID_LOG_INFO;
			break;
		case LogLevel::Warning:
			prio = ANDROID_LOG_WARN;
			break;
		case LogLevel::Error:
			prio = ANDROID_LOG_ERROR;
			break;
		case LogLevel::Fatal:
			prio = ANDROID_LOG_FATAL;
			break;
		default:
			prio = ANDROID_LOG_DEBUG;
			break;
	}
	__android_log_print(prio, "gpukit", "%s", msg.c_str());
#else
	static constexpr const char* k_colors[] = {
		[static_cast<uint8_t>(LogLevel::Trace)] = "\x1B[1m",
		[static_cast<uint8_t>(LogLevel::Info)] = "\x1B[32m",
		[static_cast<uint8_t>(LogLevel::Warning)] = "\x1B[93m",
		[static_cast<uint8_t>(LogLevel::Error)] = "\x1B[91m",
		[static_cast<uint8_t>(LogLevel::Fatal)] = "\x1B[31m",
	};
	const auto now = std::chrono::floor<std::chrono::seconds>(std::chrono::system_clock::now());
	std::clog << k_colors[static_cast<uint8_t>(level)]
			  << GPUKIT_FMT_FORMAT("[{:%H:%M:%S}] {}", now, msg) << "\x1B[0m\n";
#endif
}

} // namespace gpukit::log
