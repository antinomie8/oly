#pragma once

#include <filesystem>
#include <string>

#include "oly/log.hpp"
#include "yaml-cpp/yaml.h"

namespace fs = std::filesystem;

namespace configuration {
void load_config(std::string config_file_path);

enum class lang : std::uint8_t { latex, typst };
} // namespace configuration

struct Options {
	// the author's name in the preview and generated pdfs
	std::string author;

	// the base path where solutions are stored
	fs::path base_path;

	// the path where figures are stored, inside of base_path
	fs::path figures_dir{"figures"};

	// the markup language the solutions are typed in
	// must be one of latex or typst (default latex)
	configuration::lang lang = configuration::lang::latex;

	// language setting passed to the typst preview and preamble files.
	// must be a two or three letters language code
	std::string language = "en";

	// the text editor to use
	// autodected from $EDITOR and $VISUAL, fallback to vim
	std::string editor;

	// the pdf viewer used for 'oly gen'
	std::string pdf_viewer;

	// whether to open the generated pdf with pdf_viewer when running 'oly gen'
	bool open = true;

	// whether to confirm deletion or not in 'oly rm'
	bool confirm = false;

	// whether to only output the entries written in the current markup language in
	// 'oly list'
	bool filter_lang = true;

	// map contest names to other ones
	std::map<std::string, std::string> abbreviations{{"Shortlist", "ISL"}};

	// directory structure to store contests
	// available variables are: date, contest, year, problem and source
	std::map<std::string, std::string> contest_format{};

	std::map<std::string, std::string> contest_format_prefix{};

	// customize the colors used by the show command
	std::map<std::string, int> colorscheme{
	    {"punctuation.special", 0x7fb4ca},
	    {"punctuation.delimiter", 0x9cabca},
	    {"punctuation.bracket", 0x9cabca},
	    {"operator", 0xc0a36e},
	    {"keyword.import", 0xe46876},
	    {"keyword", 0x957fb8},
	    {"keyword.repeat", 0x957fb8},
	    {"keyword.conditional", 0x957fb8},
	    {"number", 0xd27e99},
	    {"string", 0x98bb6c},
	    {"boolean", 0xffa066},
	    {"constant", 0xffa066},
	    {"variable.member", 0xe6c384},
	    {"function.call", 0x7e9cd8},
	    {"markup.heading.1", 0x7e9cd8},
	    {"markup.heading.2", 0x7e9cd8},
	    {"markup.heading.3", 0x7e9cd8},
	    {"markup.heading.4", 0x7e9cd8},
	    {"markup.heading.5", 0x7e9cd8},
	    {"markup.heading.6", 0x7e9cd8},
	    {"markup.link.url", 0x7fb4ca},
	    {"markup.raw", 0x98bb6c},
	    {"label", 0x957fb8},
	    {"markup.raw.block", 0x98bb6c},
	    {"markup.link.label", 0x7fb4ca},
	    {"markup.link", 0x7fb4ca},
	    {"markup.math", 0xffa066},
	};

	// where to output the generated pdf
	// defaults to ${XDG_CACHE_HOME:-~/.cache}/oly/${source}
	fs::path output_directory;

	// where temporary files are written
	// defaults to ${TMPDIR:-/tmp}/oly
	fs::path tmpdir;

	// packages to import
	std::string latex_packages;
	std::string typst_packages;

	// preview is what gets put in the preview file
	// contents is the initial content in a file opened through 'oly add'
	// metadata is the metadata you get prompted for in 'oly add'
	std::string preview;
	std::string contents;
	std::string metadata;

	void update(const YAML::Node& node) {
		if (node["author"])
			author = node["author"].as<std::string>();
		if (node["base_path"])
			base_path = fs::weakly_canonical(node["base_path"].as<std::string>());
		if (node["figures_dir"])
			figures_dir = fs::weakly_canonical(node["figures_dir"].as<std::string>());
		if (node["lang"]) {
			auto s = node["lang"].as<std::string>();
			if (s == "latex")
				lang = configuration::lang::latex;
			else if (s == "typst")
				lang = configuration::lang::typst;
			else
				throw std::runtime_error("Invalid value for 'lang': " + s);
		}
		if (node["language"])
			language = node["language"].as<std::string>();
		if (node["editor"])
			editor = node["editor"].as<std::string>();
		if (node["pdf_viewer"])
			pdf_viewer = node["pdf_viewer"].as<std::string>();
		if (node["open"])
			open = node["open"].as<bool>();
		if (node["confirm"])
			confirm = node["confirm"].as<bool>();
		if (node["filter_lang"])
			confirm = node["filter_lang"].as<bool>();
		if (node["abbreviations"])
			abbreviations = node["abbreviations"].as<std::map<std::string, std::string>>();
		if (node["contest_format"])
			contest_format = node["contest_format"].as<std::map<std::string, std::string>>();
		if (node["contest_format_prefix"])
			contest_format_prefix =
			    node["contest_format_prefix"].as<std::map<std::string, std::string>>();
		if (node["colorscheme"]) {
			auto user_colors = node["colorscheme"].as<std::map<std::string, std::string>>();
			for (auto& [key, hex_str] : user_colors) {
				if (hex_str.length() == 7 && hex_str.at(0) == '#') {
					std::istringstream(hex_str.erase(0, 1)) >> std::hex >> colorscheme[key];
				} else {
					Log::ERROR(hex_str + " is not a valid hex color code !");
				}
			}
		}
		if (node["output_directory"])
			output_directory = fs::weakly_canonical(node["output_directory"].as<std::string>());
		if (node["tmpdir"])
			tmpdir = fs::weakly_canonical(node["tmpdir"].as<std::string>());
		if (node["packages"]) {
			if (node["packages"]["latex"])
				latex_packages = node["packages"]["latex"].as<std::string>();
			if (node["packages"]["typst"])
				typst_packages = node["packages"]["typst"].as<std::string>();
		}
		if (node["preview"])
			preview = node["preview"].as<std::string>();
		if (node["contents"])
			contents = node["contents"].as<std::string>();
		if (node["metadata"])
			metadata = node["metadata"].as<std::string>();
	}
};

inline Options opts;
inline std::map<std::string, std::string> shared;
inline YAML::Node metadata;
