#include <iostream>
#include <print>

#include "oly/cmds/default.hpp"
#include "oly/cmds/get_cmd.hpp"
#include "oly/config.hpp"
#include "oly/contest.hpp"
#include "oly/log.hpp"
#include "oly/utils.hpp"

#include "../version.h"

void Default::print_version() {
	std::println("{} version {}", OLY_NAME, OLY_VERSION);
	std::println("Build type: {}", OLY_BUILD_TYPE);
	std::println("Commit hash: {}", OLY_COMMIT_HASH);
}

Default::Default() {
	add("--version,-v", "print program version and related info",
	    [] { Default::print_version(); });
	add("--verify-config", "check config file", [this] {
		Config::load_config(get<std::string>("--config-file"));
		std::println("All good !");
	});
	add(
	    "--scheme", "use the scheme handler",
	    [this](std::string request) {
		    Config::load_config(get<std::string>("--config-file"));

		    if (request.empty()) {
			    std::string pb_name;
			    std::cout << "Enter problem name: " << std::flush;
			    std::getline(std::cin, pb_name);
			    fs::path pb_path = get_problem_path(pb_name);
			    if (fs::exists(pb_path)) {
				    request = "oly://edit?name=" + pb_name;
			    } else {
				    request = "oly://add?name=" + pb_name;
			    }
		    } else {
			    Log::INFO("received request: " + request);
		    }

		    std::string url =
		        request.substr(std::min(static_cast<size_t>(6), request.size()));
		    size_t mark = url.find('?');
		    size_t equals = url.find('=', mark);
		    if (mark == std::string::npos || equals == std::string::npos)
			    Log::CRITICAL("malformed query: expected format oly://cmd?name=<problem name>");

		    std::string cmd_name = url.substr(0, mark);
		    std::string pb_name = url.substr(equals + 1);

		    std::vector<std::string> args{pb_name};
		    std::unique_ptr<Command> cmd = get_cmd(cmd_name);

		    shared["cmd"] = cmd_name;
		    shared["scheme"] = "true";
		    utils::set_log_level("WARNING");
		    setenv("OLY", cmd_name.c_str(), 1);

		    cmd->parse(args);
		    return cmd->execute();
	    },
	    false);
}

int Default::execute() {
	return 0;
}
