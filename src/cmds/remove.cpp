#include <filesystem>

#include "oly/cmds/remove.hpp"
#include "oly/config.hpp"
#include "oly/contest.hpp"
#include "oly/log.hpp"
#include "oly/utils.hpp"

namespace fs = std::filesystem;

Remove::Remove() {
	add("--confirm,-i", "Prompt before deleting file", [] { opts.confirm = true; });
	add("--force,-f", "Do not prompt before deleting file", [] { opts.confirm = false; });
}

static void delete_problem(const fs::path& path) {
	if (!fs::exists(path)) {
		Log::ERROR(path.string() + " doesn't exist !");
	} else {
		if (!opts.confirm || utils::prompt_before_deletion(path)) {
			if (!fs::remove_all(path)) {
				Log::ERROR(path.string() + " couldn't be removed...");
			} else {
				utils::file::remove_empty_parents(path.parent_path(), opts.base_path);
			}
		}
	}
}

int Remove::execute() {
	if (positional_args.empty()) {
		for (const std::string& problem : utils::prompt_user_for_problems()) {
			positional_args.push_back(problem);
		}
	}

	for (const std::string& arg : positional_args) {
		delete_problem(get_problem_path(arg));
	}
	return 0;
}
