#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>

#include "oly/cmds/list.hpp"
#include "oly/config.hpp"
#include "oly/contest.hpp"
#include "oly/log.hpp"
#include "oly/utils.hpp"

List::List() {
	add("--test,-t", "Test whether get_problem_name output matches the source", false);
	add("--filter-lang", "Only output the entries written in the current markup language",
	    [] { opts.filter_lang = true; });
	add("--no-filter-lang", "Output entries regardless of the markup language used",
	    [] { opts.filter_lang = false; });
	add("--print0", "Output entries separated by NUL bytes", false);
};

std::optional<std::string>
List::parse_metadata_from_file(const fs::path& solution_path) const {
	std::ifstream solution_file(solution_path);
	if (!solution_file.is_open())
		Log::CRITICAL("Could not open " + solution_path.string() + "!");

	std::string line;
	std::string metadata;
	getline(solution_file, line);
	while (getline(solution_file, line)) {
		if (utils::is_yaml(line)) {
			metadata += (line + '\n');
		} else {
			break;
		}
	}

	try {
		std::optional<YAML::Node> yaml = utils::yaml::load(metadata, solution_path.string());
		if (yaml.has_value()) {
			YAML::Node source = yaml.value()["source"];
			if (source && source.IsScalar()) {
				return source.as<std::string>();
			}
		}
	} catch (std::exception& e) {
		Log::ERROR(e.what());
	}
	return std::nullopt;
}

int List::execute() {
	fs::path base_path = opts.base_path;
	try {
		char separator = get<bool>("--print0") ? '\0' : '\n';
		for (const auto& entry : fs::recursive_directory_iterator(base_path)) {
			if (entry.is_regular_file()) {
				if (get<bool>("--filter-lang") &&
				    entry.path().extension().string() != utils::filetype_extension())
					continue;

				std::optional<std::string> source = parse_metadata_from_file(entry.path());
				if (source.has_value()) {
					if (not get<bool>("--test") or
					    source.value() != get_problem_name(source.value())) {
						std::cout << source.value() << separator;
					}
				}
			}
		}
		std::cout.flush();
	} catch (const fs::filesystem_error& e) {
		Log::ERROR(e.what());
	}
	return 0;
}
