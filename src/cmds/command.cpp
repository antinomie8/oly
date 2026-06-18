#include <algorithm>
#include <memory>
#include <print>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

#include "oly/cmds/command.hpp"
#include "oly/config.hpp"
#include "oly/log.hpp"
#include "oly/utils.hpp"

Command::Command() {
	std::string config_home;
	const char* xdg_config_home = std::getenv("XDG_CONFIG_HOME");
	if (!xdg_config_home) {
		const char* home = std::getenv("HOME");
		if (!home) {
			Log::CRITICAL("Nor $HOME nor $XDG_CONFIG_HOME are set !");
		} else {
			config_home = static_cast<std::string>(home) + "/.config";
		}
	} else {
		config_home = static_cast<std::string>(xdg_config_home);
	}
	add("--config-file,-c", "Specify config file to use", config_home + "/oly/config.yaml");
	add("--help,-h", "Show help", [this]() { Command::print_help(); });
	add("--log-level", "Specify log level (default INFO)",
	    [](std::string level) { utils::set_log_level(level); });
	add("--lang", "Choose markup language to use", [](std::string lang) {
		if (lang != "latex" && lang != "typst") {
			Log::CRITICAL("lang needs to be one of latex or typst !");
		} else {
			opts.lang =
			    lang == "latex" ? Config::lang::latex : Config::lang::typst;
		}
	});
	add("--language", "Choose which language to use",
	    [&](std::string language) { opts.language = language; });
}

int Command::execute() {
	return 0;
}

void Command::add(std::string flags, std::string desc, void (*callback)()) {
	add(flags, std::move(desc), std::function<void()>(callback));
}

void Command::add(std::string flags, std::string desc, void (*callback)(std::string)) {
	add(flags, std::move(desc), std::function<void(std::string)>(callback));
}

bool Command::has(const std::string& flag) const {
	return lookup.contains(flag);
}

void Command::set(const std::string& flag, std::variant<bool, std::string> val) {
	auto it = lookup.find(flag);
	if (it == lookup.end())
		throw std::invalid_argument{"Unknown option: " + flag};
	it->second->value = std::move(val);
}

void Command::parse(const std::vector<std::string>& args) {
	for (size_t i = 0; i < args.size(); ++i) {
		const std::string& arg = args[i];

		// stop parsing after '--'
		if (arg == "--") {
			for (size_t j = i + 1; j < args.size(); ++j)
				positional_args.push_back(args[j]);
			return;
		}

		// Long option: --flag, --flag=value or --flag:value
		else if (arg.starts_with("--")) {
			auto eq_pos = arg.find('=');
			if (eq_pos == std::string::npos)
				eq_pos = arg.find(':');
			const std::string flag =
			    (eq_pos != std::string::npos) ? arg.substr(0, eq_pos) : arg;

			if (!has(flag))
				Log::CRITICAL("Unknown flag : " + flag, logopt::HELP | logopt::NO_PREFIX);

			const auto opt_ptr = lookup[flag];
			if (opt_ptr->requires_arg) {
				if (eq_pos != std::string::npos) {
					set(flag, arg.substr(eq_pos + 1));
				} else {
					if (i + 1 == args.size()) {
						if (opt_ptr->optional_arg) {
							set(flag, true);
						} else {
							Log::CRITICAL(flag + " requires an argument",
							              logopt::HELP | logopt::NO_PREFIX);
						}
					} else {
						set(flag, args[++i]);
					}
				}
			} else {
				set(flag, true);
			}
			if (opt_ptr->has_callback) {
				if (opt_ptr->requires_arg &&
				    std::holds_alternative<std::string>(opt_ptr->value)) {
					// Pass the argument value to the callback
					std::get<std::function<void(std::string)>>(opt_ptr->callback)(
					    std::get<std::string>(opt_ptr->value));
				} else if (opt_ptr->optional_arg) {
					std::get<std::function<void(std::string)>>(opt_ptr->callback)("");
				} else {
					std::get<std::function<void()>>(opt_ptr->callback)();
				}
			}
		}

		// Short options: -f or -abc
		else if (arg.size() > 1 && arg[0] == '-' && arg[1] != '-') {
			for (size_t j = 1; j < arg.size(); ++j) {
				const std::string short_flag = "-" + std::string(1, arg[j]);
				if (!has(short_flag))
					Log::CRITICAL("Unknown flag : " + short_flag, logopt::HELP | logopt::NO_PREFIX);

				const auto opt_ptr = lookup[short_flag];
				if (opt_ptr->requires_arg) {
					if (j + 1 < arg.size()) {
						set(short_flag, arg.substr(j + 1));
						break; // rest of string consumed as argument
					} else {
						if (i + 1 >= args.size())
							Log::CRITICAL(short_flag + " requires an argument",
							              logopt::HELP | logopt::NO_PREFIX);
						set(short_flag, args[++i]);
					}
				} else {
					set(short_flag, true);
				}
				if (opt_ptr->has_callback) {
					if (opt_ptr->requires_arg) {
						// Pass the argument value to the callback
						std::get<std::function<void(std::string)>>(opt_ptr->callback)(
						    std::get<std::string>(opt_ptr->value));
					} else {
						std::get<std::function<void()>>(opt_ptr->callback)();
					}
				}
			}
		}

		// positional arg
		else {
			positional_args.push_back(arg);
		}
	}
}

void Command::load_config_file(const std::vector<std::string>& args) {
	for (size_t i = 0; i < args.size(); ++i) {
		const std::string& flag = args[i];

		if (flag == "--")
			break; // stop parsing

		if (flag == "--config-file" || flag == "-c") {
			if (i == args.size() - 1) {
				Log::CRITICAL(args[i] + " requires an argument",
				              logopt::HELP | logopt::NO_PREFIX);
			} else {
				set(flag, args[++i]);
				break;
			}
		}
	}
	Config::load_config(get<std::string>("--config-file"));
}

void Command::print_help() const {
	if (shared["cmd"] == "default") {
		utils::print_help();
	} else {
		const std::string cmd = shared["cmd"];

		std::vector<std::string> alias_strings;
		alias_strings.reserve(storage.size());
		size_t max_len = 0;

		for (const auto& opt : storage) {
			std::string joined;
			for (size_t i = 0; i < opt->names.size(); ++i) {
				joined += opt->names[i];
				if (i + 1 < opt->names.size())
					joined += ", ";
			}
			max_len = std::max(max_len, joined.size());
			alias_strings.push_back(std::move(joined));
		}

		std::println("available arguments for {}:", cmd);

		for (size_t i = 0; i < storage.size(); ++i) {
			const auto& opt = storage[i];
			const auto& alias_str = alias_strings[i];

			std::println("{:<{}} - {}", alias_str, max_len + 5, opt->desc);
		}
	}

	std::exit(0);
}
