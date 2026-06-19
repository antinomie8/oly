#include <cstddef>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <optional>
#include <string>

#include "yaml-cpp/yaml.h"

#include "oly/config.hpp"
#include "oly/log.hpp"
#include "oly/utils.hpp"

namespace fs = std::filesystem;

static std::string get_editor() {
	const char* editor = std::getenv("EDITOR");
	if (!editor)
		editor = std::getenv("VISUAL");
	if (!editor) {
		for (const char* cand : {"vim", "nvim", "nano", "helix"}) {
			if (utils::is_executable(cand)) {
				editor = cand;
				break;
			}
		}
	}
	if (!editor)
		Log::CRITICAL("No editor found ! Set the $EDITOR or $VISUAL environment variable");

	return static_cast<std::string>(editor);
}

static bool is_valid(const YAML::Node& config) {
	bool valid = true;
	std::vector<std::string> required_fields = {"author", "base_path", "pdf_viewer"};
	std::vector<std::string> missing_fields;
	std::vector<std::string> wrong_type;

	for (const std::string& field : required_fields) {
		if (!config[field]) {
			missing_fields.push_back(field);
		} else if (!(config[field].IsScalar())) {
			wrong_type.push_back(field);
		}
	}

	auto concatenate = [](std::vector<std::string> a) -> std::string {
		std::string str = a[0];
		for (size_t i = 1; i < a.size(); i++) {
			if (i == a.size() - 1) {
				str.append(" and " + a[i]);
			} else {
				str.append(", " + a[i]);
			}
		}
		return str;
	};
	if (!missing_fields.empty()) {
		Log::ERROR(concatenate(missing_fields) + " must be configured in config.yaml");
		valid = false;
	}
	if (!wrong_type.empty()) {
		std::string msg = wrong_type.size() == 1 ? " must be a string" : " must be strings";
		Log::ERROR(concatenate(wrong_type) + msg);
		valid = false;
	}

	if (config["lang"]) {
		if (!config["lang"].IsScalar() || (config["lang"].as<std::string>() != "latex" &&
		                                   config["lang"].as<std::string>() != "typst")) {
			Log::ERROR("lang has to be one of latex or typst");
			valid = false;
		}
	}

	if (!valid) {
		Log::Wait();
	}

	return valid;
}

static void add_defaults(YAML::Node& config) {
	config["base_path"] = utils::expand_env_vars(config["base_path"].as<std::string>());

	if (config["output_directory"]) {
		config["output_directory"] =
		    utils::expand_env_vars(config["output_directory"].as<std::string>());
	} else {
		std::string cache_home;
		const char* xdg_cache_home = std::getenv("XDG_CACHE_HOME");
		if (!xdg_cache_home) {
			const char* home = std::getenv("HOME");
			if (!home) {
				Log::CRITICAL("Nor $HOME nor $XDG_CACHE_HOME are set !");
			} else {
				cache_home = static_cast<std::string>(home) + "/.cache";
			}
		} else {
			cache_home = static_cast<std::string>(xdg_cache_home);
		}
		config["output_directory"] = cache_home + "/oly";
	}

	if (config["tmpdir"]) {
		config["tmpdir"] = utils::expand_env_vars(config["tmpdir"].as<std::string>());
	} else {
		const char* tmpdir = std::getenv("TMPDIR");
		if (!tmpdir) {
			tmpdir = "/tmp";
		}
		config["tmpdir"] = static_cast<std::string>(tmpdir) + "/oly/";
	}

	if (!config["preview"] || !config["preview"]["latex"]) {
		constexpr char DEFAULT_preview_BYTES[] = {
#embed "../assets/tex/preview.tex"
		};
		constexpr size_t DEFAULT_preview_SIZE = sizeof(DEFAULT_preview_BYTES);
		std::string preview(DEFAULT_preview_BYTES, DEFAULT_preview_SIZE);
		config["preview"]["latex"] = preview;
	}
	if (!config["preview"] || !config["preview"]["typst"]) {
		constexpr char DEFAULT_preview_BYTES[] = {
#embed "../assets/typst/preview.typ"
		};
		constexpr size_t DEFAULT_preview_SIZE = sizeof(DEFAULT_preview_BYTES);
		std::string preview(DEFAULT_preview_BYTES, DEFAULT_preview_SIZE);
		config["preview"]["typst"] = preview;
	}

	if (!config["contents"] || !config["contents"]["latex"]) {
		constexpr char DEFAULT_CONTENTS_BYTES[] = {
#embed "../assets/tex/contents.tex"
		};
		constexpr size_t DEFAULT_CONTENTS_SIZE = sizeof(DEFAULT_CONTENTS_BYTES);
		std::string contents(DEFAULT_CONTENTS_BYTES, DEFAULT_CONTENTS_SIZE);
		config["contents"]["latex"] = contents;
	}
	if (!config["contents"] || !config["contents"]["typst"]) {
		constexpr char DEFAULT_CONTENTS_BYTES[] = {
#embed "../assets/typst/contents.typ"
		};
		constexpr size_t DEFAULT_CONTENTS_SIZE = sizeof(DEFAULT_CONTENTS_BYTES);
		std::string contents(DEFAULT_CONTENTS_BYTES, DEFAULT_CONTENTS_SIZE);
		config["contents"]["typst"] = contents;
	}

	if (!config["metadata"]) {
		constexpr char DEFAULT_METADATA_BYTES[] = {
#embed "../assets/metadata.yaml"
		};
		constexpr size_t DEFAULT_METADATA_SIZE = sizeof(DEFAULT_METADATA_BYTES);
		std::string metadata(DEFAULT_METADATA_BYTES, DEFAULT_METADATA_SIZE);
		config["metadata"] = metadata;
	}
}

namespace Config {
void load_config(std::string config_file_path) {
	const fs::path config_file = fs::absolute(utils::expand_env_vars(config_file_path));

	opts.editor = get_editor();
	utils::input_file config(config_file);

	// default config
	if (!fs::exists(config_file)) {
		constexpr char DEFAULT_CONFIG_BYTES[] = {
#embed "../assets/config.yaml"
		};
		constexpr size_t DEFAULT_CONFIG_SIZE = sizeof(DEFAULT_CONFIG_BYTES);
		std::string default_config(DEFAULT_CONFIG_BYTES, DEFAULT_CONFIG_SIZE);

		config.contents = default_config;
		config.create();
		config.edit();
	}

	// load config
	std::optional<YAML::Node> userconfig = utils::yaml::load(config_file);
	while (true) {
		while (!userconfig || !is_valid(userconfig.value())) {
			config.edit();
			userconfig = utils::yaml::load(config_file);
		}

		add_defaults(userconfig.value());
		try {
			opts.update(userconfig.value());
			break;
		} catch (const std::exception& e) {
			Log::ERROR(e.what(), logopt::WAIT);
		}
	}
}
} // namespace Config
