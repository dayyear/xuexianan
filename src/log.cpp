#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>

#include "log.h"

class A {
public:
	A() {
		std::shared_ptr<spdlog::sinks::stdout_color_sink_mt> console_sink;
		std::shared_ptr<spdlog::sinks::daily_file_sink_mt> file_sink;
		console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
		console_sink->set_level(spdlog::level::info);
		console_sink->set_pattern("[%^%l%$] %v");

		system("mkdir 2>NUL log");
		file_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>("log/log.txt", 0, 5);
		file_sink->set_level(spdlog::level::trace);
		file_sink->set_pattern("[%X] [%^%l%$] %v");

		logger = new spdlog::logger("multi_sink", { console_sink, file_sink });
		logger->set_level(spdlog::level::trace);
		logger->flush_on(spdlog::level::trace);
	}
};
static A a;

spdlog::logger *logger;
