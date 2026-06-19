#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
#include <unistd.h>
#include <vector>

#include <tree_sitter/api.h>

#include "oly/cmds/show.hpp"
#include "oly/config.hpp"
#include "oly/constants.hpp"
#include "oly/contest.hpp"
#include "oly/log.hpp"
#include "oly/utils.hpp"

namespace fs = std::filesystem;

Show::Show() {
	add("--color", "When to use terminal colours (always, auto, never)", "auto");
}

// external C symbols from the tree-sitter parsers
extern "C" const TSLanguage* tree_sitter_latex();
extern "C" const TSLanguage* tree_sitter_typst();

// treesitter queries
constexpr const char LATEX_QUERIES[] = {
#embed "../../assets/tex/highlights.scm"
};
constexpr size_t LATEX_QUERIES_SIZE = sizeof(LATEX_QUERIES);
static std::string latex_queries(LATEX_QUERIES, LATEX_QUERIES_SIZE);
constexpr const char TYPST_QUERIES[] = {
#embed "../../assets/typst/highlights.scm"
};
constexpr size_t TYPST_QUERIES_SIZE = sizeof(TYPST_QUERIES);
static std::string typst_queries(TYPST_QUERIES, TYPST_QUERIES_SIZE);

static std::string to_ansi(int color) {
	uint8_t r, g, b;
	r = (color >> 16) & 0xFF;
	g = (color >> 8) & 0xFF;
	b = color & 0xFF;

	return std::format("\x1b[38;2;{};{};{}m", r, g, b);
}

static std::string map_capture(const std::string& name) {
	if (name == "markup.strong") {
		return Color::BOLD;
	} else if (name == "markup.italic") {
		return Color::ITALIC;
	}
	return opts.colorscheme.contains(name) ? to_ansi(opts.colorscheme[name]) : "";
}

static std::string colorize(const std::string& input) {
	if (input.empty())
		return input;

	static TSParser* parser = ts_parser_new();

	TSQuery* query = nullptr;
	const TSLanguage* lang;

	static auto get_query = [](const TSLanguage* l, const std::string& src) {
		uint32_t offset;
		TSQueryError err;
		return ts_query_new(l, src.c_str(), src.length(), &offset, &err);
	};
	if (opts.lang == Config::lang::latex) {
		// hella slow for some reason
		static TSQuery* q_latex = get_query(tree_sitter_latex(), latex_queries);
		query = q_latex;
		lang = tree_sitter_latex();
	} else if (opts.lang == Config::lang::typst) {
		static TSQuery* q_typst = get_query(tree_sitter_typst(), typst_queries);
		query = q_typst;
		lang = tree_sitter_typst();
	}

	if (!query)
		return input;

	ts_parser_set_language(parser, lang);

	TSTree* tree = ts_parser_parse_string(parser, nullptr, input.c_str(), input.length());
	TSNode root = ts_tree_root_node(tree);

	std::vector<std::string> color_buffer(input.length(), "");
	TSQueryCursor* cursor = ts_query_cursor_new();
	ts_query_cursor_exec(cursor, query, root);

	TSQueryMatch match;
	while (ts_query_cursor_next_match(cursor, &match)) {
		for (uint16_t i = 0; i < match.capture_count; ++i) {
			uint32_t name_len;
			const char* name_ptr =
			    ts_query_capture_name_for_id(query, match.captures[i].index, &name_len);
			std::string esc = map_capture(std::string(name_ptr, name_len));

			if (esc.empty())
				continue;

			uint32_t start = ts_node_start_byte(match.captures[i].node);
			uint32_t end = ts_node_end_byte(match.captures[i].node);
			for (uint32_t j = start; j < end && j < color_buffer.size(); ++j) {
				color_buffer[j] = esc;
			}
		}
	}

	// reconstruct string
	std::string result;
	result.reserve(input.length() * 1.2);
	std::string last_color = "";
	for (size_t i = 0; i < input.length(); ++i) {
		if (color_buffer[i] != last_color) {
			result += color_buffer[i].empty() ? Color::RESET : color_buffer[i];
			last_color = color_buffer[i];
		}
		result += input[i];
	}
	result += Color::RESET;

	ts_query_cursor_delete(cursor);
	ts_tree_delete(tree);
	return result;
}

std::string Show::process(std::string& input) const {
	input = utils::trim_newlines(input);
	std::string color_opt = get<std::string>("--color");
	if (color_opt == "auto") {
		color_opt = isatty(STDOUT_FILENO) ? "always" : "never";
	}
	if (color_opt == "always") {
		input = colorize(input);
	} else if (color_opt != "never") {
		Log::WARNING(
		    "Invalid value for --color: should be one of auto, never or always (received " +
		    color_opt + ")");
	}
	return input + '\n';
}

std::string Show::get_statement(const fs::path& pb) const {
	std::ifstream file(pb);
	if (!file.is_open())
		throw std::runtime_error("Could not open " + pb.string() + "!");

	std::string pb_statement;
	std::string line;
	while (getline(file, line)) {
		if (!utils::is_yaml(line) && !utils::should_ignore(line) &&
		    !utils::is_package_import(line)) {
			if (!utils::is_separator(line)) {
				pb_statement += (line + '\n');
			}
			break;
		}
	}
	while (getline(file, line)) {
		if (utils::is_separator(line)) {
			return process(pb_statement);
		} else {
			pb_statement += (line + '\n');
		}
	}

	return process(pb_statement);
}

bool Show::print_statement(const fs::path& source_path) const {
	if (!fs::exists(source_path)) {
		Log::ERROR(source_path.string() + " does not exist !");
		return false;
	} else {
		try {
			std::cout << get_statement(source_path);
		} catch (std::runtime_error& e) {
			Log::ERROR(e.what());
			return false;
		}
		return true;
	}
}

int Show::execute() {
	if (positional_args.empty()) {
		for (const std::string& problem : utils::prompt_user_for_problems()) {
			positional_args.push_back(problem);
		}
	}

	bool success = true;
	for (const std::string& pb : positional_args) {
		success = success && print_statement(get_problem_solution_path(pb));
		if (&pb != &positional_args.back()) {
			std::cout << std::endl;
		}
	}
	return success ? 0 : 1;
}
