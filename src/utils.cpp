#include <cstdlib>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <print>
#include <regex>
#include <sys/wait.h>
#include <unistd.h>

#include "oly/config.hpp"
#include "oly/constants.hpp"
#include "oly/log.hpp"
#include "oly/utils.hpp"

namespace fs = std::filesystem;

namespace utils {
void print_help() {
	std::println("usage: oly <cmd> [args [...]].\n");
	std::println(R"(Available subcommands:
    add                          - add a problem to the database
    edit                         - edit an entry from the database
    gen                          - generate a PDF from a problem
    search                       - search problems by contest, metadata...
    show                         - print a problem statement
    list                         - list problems in the database
    alias                        - link a problem to another one
    rm                           - remove a problem and its solution file
    mv                           - rename a problem
  Run oly <cmd> --help for more information regarding a specific subcommand
)");
	std::println(R"(Arguments:
    --help              -h       - Show this help message
    --config-file FILE  -c FILE  - Specify config file to use
    --verify-config              - Check whether the config has any errors
    --version           -v       - Print this binary's version
    --log-level LEVEL            - Set the log level)");
}

std::string expand_vars(const std::string& str, bool expand_config_vars,
                        bool expand_env_vars) {
	auto formatter = [&](const std::string& match) -> std::string {
		if (expand_config_vars && metadata[match] && metadata[match].IsScalar()) {
			return metadata[match].as<std::string>();
		} else if (expand_config_vars && shared.contains(match)) {
			return shared[match];
		} else if (expand_env_vars) {
			const char* s = getenv(match.c_str());
			return (s == nullptr ? "" : s);
		} else {
			return "";
		}
	};

	std::string fmt = utils::expand_vars(str, formatter);

	if (expand_env_vars && fmt.starts_with("~/")) {
		const char* home = getenv("HOME");
		fmt = std::string(home == nullptr ? "" : home) + fmt.substr(1);
	}

	return fmt;
};

std::string expand_env_vars(const std::string& str) {
	return utils::expand_vars(str, false, true);
}

std::string filetype_extension() {
	return opts.lang == configuration::lang::latex ? ".tex" : ".typ";
}

void set_log_level(std::string level) {
	std::ranges::transform(level, level.begin(),
	                       [](unsigned char c) { return std::toupper(c); });

	if (level == "CRITICAL") {
		Log::log_level = severity::CRITICAL;
	} else if (level == "ERROR") {
		Log::log_level = severity::ERROR;
	} else if (level == "WARNING") {
		Log::log_level = severity::WARNING;
	} else if (level == "INFO") {
		Log::log_level = severity::INFO;
	} else if (level == "HINT") {
		Log::log_level = severity::HINT;
	} else if (level == "DEBUG") {
		Log::log_level = severity::DEBUG;
	} else if (level == "TRACE") {
		Log::log_level = severity::TRACE;
	} else {
		Log::ERROR(level + "is not a valid log level. Using default severity INFO.");
		Log::log_level = severity::INFO;
	}
}

bool is_separator(const std::string& line) {
	std::regex separator_pattern;
	if (opts.lang == configuration::lang::latex) {
		separator_pattern = R"(^\\hrulebar\s*$)";
	} else {
		separator_pattern = R"(^#divider\(\)\s*$)";
	}
	return std::regex_match(line, separator_pattern);
}

bool is_yaml(const std::string& line) {
	std::regex yaml_pattern(R"(^[A-Za-z]+:\s*.+$)");
	return std::regex_match(line, yaml_pattern);
}

bool should_ignore(const std::string& line) {
	if (opts.lang == configuration::lang::typst) {
		if (line.starts_with("#import")) {
			return true;
		}
	}
	if (std::regex_match(line, std::regex(R"(^\s*$)"))) {
		return true;
	}
	return false;
}

bool copy_dir(const fs::path& from, const std::string& to) {
	std::error_code ec;

	if (!fs::exists(from, ec) || !fs::is_directory(from, ec)) {
		if (fs::exists(to, ec)) {
			fs::remove(to, ec);
			return true;
		} else {
			return false;
		}
	}

	if (!fs::exists(to, ec)) {
		fs::create_directories(to, ec);
		if (ec)
			return false;
	}

	fs::copy(from, to, fs::copy_options::recursive | fs::copy_options::overwrite_existing,
	         ec);

	if (ec) {
		// only DEBUG because it can be some useless error like 'file exists'
		Log::DEBUG("utils::copy_dir: Error copying: " + ec.message());
		return false;
	}

	return true;
}

int run(const std::vector<std::string>& args, bool silent) {
	std::vector<char*> c_args(args.size() + 1);
	for (size_t i = 0; i < args.size(); ++i)
		c_args[i] = const_cast<char*>(args[i].c_str());
	c_args[c_args.size() - 1] = nullptr;

	pid_t pid = fork();

	if (pid == 0) {
		// child
		if (silent) {
			int devnull = open("/dev/null", O_WRONLY);
			if (devnull != -1) {
				dup2(devnull, STDOUT_FILENO);
				dup2(devnull, STDERR_FILENO);
				close(devnull);
			}
		}

		execvp(c_args[0], c_args.data());
		_exit(127); // exec failed
	}

	int status;
	waitpid(pid, &status, 0);

	if (WIFEXITED(status))
		return WEXITSTATUS(status);

	if (WIFSIGNALED(status))
		return 128 + WTERMSIG(status);

	return -1;
}

bool is_executable(const std::string& cmd) {
	const char* path_env = std::getenv("PATH");
	if (!path_env)
		return false;

	std::stringstream ss(path_env);
	std::string dir;

	while (std::getline(ss, dir, ':')) {
		std::string full = dir + "/" + cmd;
		if (access(full.c_str(), X_OK) == 0)
			return true;
	}
	return false;
}

std::vector<std::string> prompt_user_for_problems() {
	for (const auto& program : {std::string("fzf"), std::string("oly")})
		if (!utils::is_executable(program))
			Log::CRITICAL(program + " is not executable");

	// TODO: don't spawn an oly process
	std::string cmd = "oly list --print0 | \
	                  fzf --read0 --print0 --multi --preview 'oly show --color=always {}'";

	FILE* pipe = popen(cmd.c_str(), "r");
	if (!pipe)
		Log::CRITICAL("popen() failed");

	std::vector<std::string> result;
	std::string current;

	char buf[4096];
	size_t n;

	while ((n = fread(buf, 1, sizeof(buf), pipe)) > 0) {
		for (size_t i = 0; i < n; ++i) {
			if (buf[i] == '\0') {
				result.push_back(current);
				current.clear();
			} else {
				current.push_back(buf[i]);
			}
		}
	}

	pclose(pipe);

	return result;
}

bool prompt_before_deletion(const fs::path& path) {
	std::print("Are you sure you want to remove {}{}{} ? [y/n] ", Color::RED, path.string(),
	           Color::RESET);

	std::string input;
	if (!std::getline(std::cin, input)) {
		return false; // EOF or error
	}

	if (input.empty()) {
		return false; // default is No
	}

	char c = static_cast<char>(std::tolower(input[0]));
	return c == 'y';
}

input_file::input_file(fs::path filepath, bool remove)
    : filepath(filepath), remove(remove) {}

input_file::input_file(fs::path filepath, std::string contents, bool remove)
    : filepath(filepath), contents(contents), remove(remove) {
	create();
	edit();
};
input_file::~input_file() {
	if (remove && fs::exists(filepath)) {
		std::error_code ec;
		fs::remove(filepath, ec);
	}
}

void input_file::create() {
	utils::file::create(filepath, contents);
}

void input_file::edit() {
	utils::file::edit(filepath);
}

std::string input_file::lines(bool ignore_blank) {
	std::ifstream file(filepath);
	std::string lines;
	std::string line;
	if (!file.is_open())
		Log::CRITICAL("unable to open " + filepath.string() + "!");

	if (ignore_blank) {
		const std::regex reg("^\\s*$");
		while (getline(file, line)) {
			if (!regex_search(line, reg)) {
				lines.append(line + '\n');
				break;
			}
		}
	}

	while (getline(file, line)) {
		lines.append(line + '\n');
	}
	return lines;
}

namespace file {
void create(const fs::path& filepath, const std::string& contents) {
	fs::create_directories(filepath.parent_path());
	std::ofstream out(filepath);
	if (!out) {
		Log::ERROR("Could not open file: " + filepath.string(), logopt::WAIT);
	}
	out << contents;
	out.close();
}

void edit(const fs::path& filepath) {
	utils::run({opts.editor, filepath.string()});
}

void remove_empty_parents(fs::path path, const fs::path& base_path) {
	std::error_code ec;

	while (!path.empty() && path != base_path) {
		if (!fs::exists(path, ec) || !fs::is_directory(path, ec))
			break;

		if (!fs::is_empty(path, ec))
			break;

		if (!fs::remove(path, ec))
			break;

		path = path.parent_path();
	}
}
} // namespace file

namespace yaml {
std::optional<YAML::Node> load(const fs::path& filepath) noexcept {
	try {
		YAML::Node data = YAML::LoadFile(filepath);
		return data;
	} catch (const YAML::ParserException& e) {
		Log::ERROR("YAML syntax error in file " + filepath.string() + ": " +
		               static_cast<std::string>(e.what()),
		           logopt::WAIT);
	} catch (const YAML::BadFile& e) {
		Log::ERROR("Could not open file: " + filepath.string(), logopt::WAIT);
	}
	return std::nullopt;
}

std::optional<YAML::Node> load(const std::string& yaml, std::string source) noexcept {
	try {
		YAML::Node data = YAML::Load(yaml);
		return data;
	} catch (const YAML::ParserException& e) {
		if (!source.empty())
			source = " in file " + source;
		Log::ERROR("YAML syntax error" + source + ": " + static_cast<std::string>(e.what()),
		           logopt::WAIT);
	}
	return std::nullopt;
}

void merge(YAML::Node& base, const YAML::Node& extend, bool override) {
	if (!extend.IsDefined())
		return;

	if (extend.IsMap()) {
		for (auto it : extend) {
			const std::string key = it.first.as<std::string>();
			const YAML::Node& value = it.second;
			if (override || !base[key]) {
				base[key] = value;
			}
		}
	}
}
void merge_metadata(const YAML::Node& extend, bool override) {
	utils::yaml::merge(metadata, extend, override);
}
} // namespace yaml

namespace preview {
void create_preview_file(const std::string& source) {
	fs::path preview_file_path(opts.tmpdir / source / ("preview" + filetype_extension()));
	shared["packages"] =
	    opts.lang == configuration::lang::latex ? opts.latex_packages : opts.typst_packages;
	utils::file::create(preview_file_path, utils::expand_vars(opts.preview));
}
} // namespace preview

namespace figures {
bool copy(const fs::path& tmp_path, const fs::path& pb_path) {
	const fs::path from{pb_path / opts.figures_dir};
	const fs::path to{tmp_path / opts.figures_dir};
	return utils::copy_dir(from, to);
}

bool save(const fs::path& tmp_path, const fs::path& pb_path) {
	const fs::path from{tmp_path / opts.figures_dir};
	const fs::path to{pb_path / opts.figures_dir};
	return utils::copy_dir(from, to);
}
}; // namespace figures

} // namespace utils
