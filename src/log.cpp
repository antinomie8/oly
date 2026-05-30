#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <print>
#include <string>
#include <unistd.h>

#include "oly/config.hpp"
#include "oly/constants.hpp"
#include "oly/log.hpp"
#include "oly/utils.hpp"

bool operator<(severity a, severity b) {
	return static_cast<unsigned>(a) < static_cast<unsigned>(b);
}
bool operator>(severity a, severity b) {
	return static_cast<unsigned>(a) > static_cast<unsigned>(b);
}
bool operator<=(severity a, severity b) {
	return static_cast<unsigned>(a) <= static_cast<unsigned>(b);
}
bool operator>=(severity a, severity b) {
	return static_cast<unsigned>(a) >= static_cast<unsigned>(b);
}

logopt operator|(logopt a, logopt b) {
	return static_cast<logopt>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}
logopt& operator|=(logopt& a, logopt b) {
	a = a | b;
	return a;
}

static constexpr bool has_option(logopt opts, logopt flag) {
	return (static_cast<unsigned>(opts) & static_cast<unsigned>(flag)) != 0;
}

static constexpr std::string severity_name(severity lvl) {
	switch (lvl) {
	case severity::CRITICAL:
		return "CRITICAL";
	case severity::ERROR:
		return "ERROR";
	case severity::WARNING:
		return "WARNING";
	case severity::INFO:
		return "INFO";
	case severity::HINT:
		return "HINT";
	case severity::DEBUG:
		return "DEBUG";
	case severity::TRACE:
		return "TRACE";
	default:
		return "";
	}
}
static constexpr std::string severity_color(severity lvl) {
	if (!isatty(STDOUT_FILENO))
		return "";

	switch (lvl) {
	case severity::CRITICAL:
		return "\x1b[30m\x1b[48;5;196m";
	case severity::ERROR:
		return Color::RED;
	case severity::WARNING:
		return Color::YELLOW;
	case severity::INFO:
		return Color::BLUE;
	case severity::HINT:
		return Color::GREEN;
	case severity::DEBUG:
		return Color::GRAY;
	case severity::TRACE:
		return Color::FAINT;
	default:
		return "";
	}
}

namespace Log {
static void log_stderr(severity level, const std::string& message, logopt opts) {
	if (!has_option(opts, logopt::NO_PREFIX))
		std::print(std::cerr, "{}{}{}: ", severity_color(level), severity_name(level),
		           Color::RESET);

	std::println(std::cerr, "{}", message);

	if (has_option(opts, logopt::HELP)) {
		int padding =
		    has_option(opts, logopt::NO_PREFIX) ? 0 : severity_name(level).length() + 2;
		std::string cmd_str =
		    shared.contains("cmd") && shared["cmd"] != "default" ? shared["cmd"] + " " : "";
		std::println(std::cerr, "{:{}}use oly {}--help for more information", "", padding,
		             cmd_str);
	}
}
static void notify_send(severity level, const std::string& message, logopt opts) {
	std::string severity = severity_name(level);
	std::ranges::transform(severity, severity.begin(), ::tolower);

	static std::string data_home;
	if (data_home.empty()) {
		char* xdg_data_home = std::getenv("XDG_DATA_HOME");
		if (xdg_data_home) {
			data_home = static_cast<std::string>(xdg_data_home);
		} else {
			const char* home = std::getenv("HOME");
			if (xdg_data_home) {
				data_home = static_cast<std::string>(home) + "/.local/share";
			}
		}
	}

	std::string cmd_str =
	    shared.contains("cmd") && shared["cmd"] != "default" ? shared["cmd"] + " " : "";
	utils::run({
	    "notify-send",
	    "--app-name",
	    "oly",
	    "--icon=" + data_home + "/icons/hicolor/48x48/apps/oly.png",
	    "--category=" + severity,
	    cmd_str + severity,
	    message,
	});
}

static void Log(severity level, const std::string& message, logopt opts) {
	if (level < log_level)
		return;

	if (!shared.contains("scheme")) {
		log_stderr(level, message, opts);
	} else {
		notify_send(level, message, opts);
	}

	if (has_option(opts, logopt::WAIT)) {
		Wait();
	}
}

void Wait() {
	std::print(std::cerr, "\033[0;90mPress Enter to continue...\033[0m");
	std::cerr.flush();
	std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
}

void CRITICAL(const std::string& message, logopt opts) {
	Log(severity::CRITICAL, message, opts);
	std::exit(EXIT_FAILURE);
}
void ERROR(const std::string& message, logopt opts) {
	Log(severity::ERROR, message, opts);
}
void WARNING(const std::string& message, logopt opts) {
	Log(severity::WARNING, message, opts);
}
void INFO(const std::string& message, logopt opts) {
	Log(severity::INFO, message, opts);
}
void DEBUG(const std::string& message, logopt opts) {
	Log(severity::DEBUG, message, opts);
}
void TRACE(const std::string& message, logopt opts) {
	Log(severity::TRACE, message, opts);
}
} // namespace Log
