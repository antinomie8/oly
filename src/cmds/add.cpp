#include <filesystem>
#include <string>
#include <vector>

#include "oly/cmds/add.hpp"
#include "oly/config.hpp"
#include "oly/contest.hpp"
#include "oly/log.hpp"
#include "oly/utils.hpp"

namespace fs = std::filesystem;

Add::Add() {
	add("--overwrite,-o", "Overwrite previous database entry for the problem", false);
}

std::string Add::get_solution_body(const fs::path& base_path,
                                   const std::string& source) const {
	utils::preview::create_preview_file(source);
	std::string input =
	    utils::input_file(base_path / ("solution" + utils::filetype_extension()),
	                      utils::expand_vars(opts.contents), false)
	        .lines(true);
	return input;
}

YAML::Node Add::get_solution_metadata(const fs::path& base_path) const {
	utils::input_file file(base_path / "metadata.yaml", utils::expand_vars(opts.metadata));

	auto metadata = utils::yaml::load(file.filepath);
	while (!metadata) {
		file.edit();
		metadata = utils::yaml::load(file.filepath);
	}
	return metadata.value();
}

void Add::create_solution_file(const fs::path& path, const std::string& body,
                               const YAML::Node& metadata) const {
	YAML::Emitter out;
	out << metadata;

	std::string contents =
	    (std::string(opts.lang == Config::lang::typst ? "/*" : "\\iffalse") + "\n" +
	     std::string(out.c_str()) + "\n" +
	     (opts.lang == Config::lang::typst ? "*/" : "\\fi") + ("\n\n" + body));

	utils::file::create(path, contents);
}

void Add::add_problem(const std::string& source) const {
	const fs::path pb = get_problem_path(source);
	std::string pb_name = get_problem_name(source);

	if (!get<bool>("--overwrite") && fs::exists(pb)) {
		Log::ERROR("cannot add " + pb_name + ": entry already present in database" + "\n" +
		           "Use oly edit " + pb_name + " to edit it" + "\n" +
		           "Or use --overwrite/-o to ignore this");
		return;
	}
	shared["source"] = pb_name;

	const fs::path tmp_path = opts.tmpdir / pb_name;
	std::string body = get_solution_body(tmp_path, pb_name);
	YAML::Node metadata = get_solution_metadata(tmp_path);

	utils::figures::save(tmp_path, pb);
	create_solution_file(get_problem_solution_path(source), body, metadata);
}

int Add::execute() {
	if (positional_args.empty()) {
		Log::ERROR("Expected problem name", logopt::HELP | logopt::NO_PREFIX);
		return 1;
	}

	for (const std::string& source : positional_args)
		add_problem(source);

	return 0;
}
