#pragma once

#include <filesystem>
#include <regex>
#include <string>

#include "yaml-cpp/yaml.h"

namespace fs = std::filesystem;

namespace utils {
void print_help();

[[nodiscard]]
std::string expand_vars(const std::string& str, auto&& f)
  requires requires(std::string& s) {
	  { f(s) } -> std::convertible_to<std::string>;
  };

[[nodiscard]]
std::string expand_vars(const std::string& str, bool expand_config_vars = true,
                        bool expand_env_vars = true);

[[nodiscard]]
std::string expand_env_vars(const std::string& str);

[[nodiscard]]
std::string filetype_extension();

void set_log_level(std::string level);

[[nodiscard]]
bool is_separator(const std::string& line);

[[nodiscard]]
std::string trim_newlines(std::string& str);

[[nodiscard]]
bool is_yaml(const std::string& line);

[[nodiscard]]
bool should_ignore(const std::string& line);

bool copy_dir(const fs::path& from, const std::string& to);

int run(const std::vector<std::string>& args, bool silent = false, bool detach = false);

[[nodiscard]]
bool is_executable(const std::string& program);

[[nodiscard]]
std::vector<std::string> prompt_user_for_problems();

[[nodiscard]]
bool prompt_before_deletion(const fs::path& path);

struct input_file {
	fs::path filepath;
	std::string contents;
	bool remove;

	input_file(fs::path filepath, bool remove = false);
	input_file(fs::path filepath, std::string contents, bool remove = true);
	~input_file();

	void create();
	void edit();

	[[nodiscard]]
	std::string lines(bool ignore_blank = false);
};

namespace file {
void create(const fs::path& filepath, const std::string& contents = "");

void edit(const fs::path& filepath);

void remove_empty_parents(fs::path path, const fs::path& base_path);
} // namespace file

namespace yaml {
[[nodiscard]]
std::optional<YAML::Node> load(const fs::path& filepath) noexcept;

[[nodiscard]]
std::optional<YAML::Node> load(const std::string& yaml, std::string source = "") noexcept;

void merge(YAML::Node& from, const YAML::Node& extend, bool override = true);
} // namespace yaml

namespace preview {
void create_preview_file(const std::string& source);
}

namespace figures {
bool copy(const fs::path& tmp_path, const fs::path& pb_path);

bool save(const fs::path& tmp_path, const fs::path& pb_path);
} // namespace figures

} // namespace utils

std::string utils::expand_vars(const std::string& str, auto&& f)
  requires requires(std::string& s) {
	  { f(s) } -> std::convertible_to<std::string>;
  }
{
	std::string result;

	static std::regex var(R"(\$\{([^}]+)\})");
	std::sregex_iterator it(str.begin(), str.end(), var);
	std::sregex_iterator end;

	size_t last = 0;

	for (; it != end; ++it) {
		const std::smatch& match = *it;

		result.append(str, last, match.position() - last);
		result.append(f(match[1].str()));

		last = match.position() + match.length();
	}

	result.append(str, last, std::string::npos); // NOLINT

	return result;
}
