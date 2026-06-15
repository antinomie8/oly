#include <exception>
#include <fstream>

#include "oly/cmds/edit.hpp"
#include "oly/config.hpp"
#include "oly/contest.hpp"
#include "oly/log.hpp"
#include "oly/utils.hpp"

Edit::Edit() = default;

static std::string parse_metadata_and_return_content(const fs::path& solution_path) {
	std::ifstream solution_file(solution_path);
	if (!solution_file.is_open())
		Log::CRITICAL("Could not open " + solution_path.string() + "!");

	std::string solution;
	std::string line;
	std::string metadata;
	getline(solution_file, line);
	solution += (line + '\n');
	while (getline(solution_file, line)) {
		solution += (line + '\n');
		if (!utils::is_yaml(line)) {
			break;
		} else {
			metadata += (line + '\n');
		}
	}
	while (getline(solution_file, line)) {
		solution += (line + '\n');
	}

	try {
		utils::yaml::merge_metadata(YAML::Load(metadata));
	} catch (std::exception& e) {
		Log::ERROR(e.what());
	}

	return solution;
}

static std::string get_solution(const fs::path& solution_path,
                                const std::string& source) {
	std::string solution = parse_metadata_and_return_content(solution_path);

	utils::preview::create_preview_file(source);

	const fs::path tmp_path = static_cast<fs::path>(opts.tmpdir / source);

	utils::figures::copy(tmp_path, solution_path.parent_path());

	std::string input =
	    utils::input_file(tmp_path / ("solution" + utils::filetype_extension()), solution,
	                      false)
	        .lines(true);

	utils::figures::save(tmp_path, solution_path.parent_path());

	return input;
}

static void edit_problem(const std::string& pb) {
	fs::path solution_path = get_problem_solution_path(pb);
	std::string source = get_problem_name(pb);
	shared["source"] = source;
	std::string sol = get_solution(solution_path, source);
	utils::file::create(solution_path, sol);
}

int Edit::execute() {
	if (positional_args.empty()) {
		for (const std::string& problem : utils::prompt_user_for_problems()) {
			positional_args.push_back(problem);
		}
	}

	for (const std::string& source : positional_args)
		edit_problem(source);

	return 0;
}
