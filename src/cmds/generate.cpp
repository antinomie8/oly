#include <exception>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

#include "oly/cmds/generate.hpp"
#include "oly/config.hpp"
#include "oly/contest.hpp"
#include "oly/log.hpp"
#include "oly/utils.hpp"

namespace fs = std::filesystem;

Generate::Generate() {
	add("--open", "Open the generated pdf", [] { opts.open = true; });
	add("--no-open", "Do not open the generated pdf", [] { opts.open = false; });
	add("--clean", "Remove auxiliary files", false);
	add("--no-pdf", "Only generate a source file", false);
	add("--no-source", "Remove the source file and induce --clean", false);
	add("--cwd", "Create the pdf in the current directory", false);
	add("--print-path,-p",
	    "Print the path of the directory where the pdf will be generated", false);
	add("--clear-cache", "Clear the cache", false);
	add("--regen", "Regenerate the pdf even if it was cached", false);
	add("--all", "Generate a pdf for each solution file", false);
}

std::vector<std::string> Generate::get_solution_bodies(const fs::path& source) {
	std::ifstream file(source);
	if (!file.is_open())
		throw std::runtime_error("Could not open " + source.string() + "!");

	std::vector<std::string> bodies;
	std::string body;
	std::string line;
	while (getline(file, line)) {
		if (!utils::is_yaml(line) && !utils::should_ignore(line)) {
			if (opts.lang == Config::lang::latex && line.starts_with("\\usepackage")) {
				body += (line + '\n');
			} else if (opts.lang == Config::lang::typst && line.starts_with("#import")) {
				body += (line + '\n');
			} else {
				bodies.push_back(body);
				body = "";
				break;
			}
		}
	}

	do {
		if (utils::is_separator(line)) {
			bodies.push_back(body);
			body = "";
		} else {
			body += (line + '\n');
		}
	} while (getline(file, line));
	bodies.push_back(body);

	return bodies;
}

YAML::Node Generate::get_solution_metadata(const fs::path& source) {
	std::ifstream file(source);
	if (!file.is_open())
		throw std::runtime_error("Could not open " + source.string());

	std::string yaml;
	std::string line;
	getline(file, line);
	while (getline(file, line)) {
		if (utils::is_yaml(line))
			yaml += (line + '\n');
		else
			break;
	}
	std::optional<YAML::Node> data = utils::yaml::load(yaml);
	if (!data) {
		Log::ERROR("Could not get metadata from " + source.string());
		return YAML::Node();
	}
	return data.value();
}

void Generate::create_pdf(const std::vector<std::string>& problems) {
	std::string source = shared["source"];
	fs::path output_file_path(fs::path(utils::expand_vars(opts.output_directory)) / source /
	                          (source + utils::filetype_extension()));

	bool regenerate = get<bool>("--regen");
	if (!regenerate) {
		if (fs::exists(output_file_path)) {
			auto t_output = fs::last_write_time(output_file_path);
			fs::file_time_type t_input = t_output;
			for (const auto& problem : problems) {
				t_input = max(t_input, fs::last_write_time(get_problem_solution_path(problem)));
			}
			regenerate = t_input > t_output;
		} else {
			regenerate = true;
		}
	}
	if (regenerate) {
		try {
			if (opts.lang == Config::lang::latex) {
				create_latex_file(problems, output_file_path);
				compile_latex_file(problems, output_file_path);
			} else if (opts.lang == Config::lang::typst) {
				create_typst_file(problems, output_file_path);
				compile_typst_file(problems, output_file_path);
			}
		} catch (const std::exception& e) {
			Log::ERROR("Error generating " + source + ": " + e.what());
		}
	} else {
		utils::run({opts.pdf_viewer, output_file_path.replace_extension(".pdf")}, true, true);
	}
}

void Generate::create_latex_file(const std::vector<std::string>& problems,
                                 const fs::path& latex_file_path) {
	fs::create_directories(latex_file_path.parent_path());
	std::ofstream out(latex_file_path);

	constexpr char LATEX_PREAMBLE[] = {
#embed "../../assets/tex/preamble.tex"
	};
	constexpr size_t LATEX_PREAMBLE_SIZE = sizeof(LATEX_PREAMBLE);
	std::string latex_preamble(LATEX_PREAMBLE, LATEX_PREAMBLE_SIZE);
	shared["packages"] = opts.latex_packages;
	out << utils::expand_vars(latex_preamble);

	for (const std::string& problem : problems) {
		const fs::path pb_path = get_problem_solution_path(problem);
		std::vector<std::string> bodies = get_solution_bodies(pb_path);
		YAML::Node metadata = get_solution_metadata(pb_path);

		out << bodies[0]; // packages

		if (problems.size() > 1)
			out << "\\begin{problem}";
		else
			out << "\\begin{problem*}";
		if (metadata["source"])
			out << " [" << metadata["source"] << "]";
		out << "\n";
		if (bodies.size() > 1)
			out << bodies[1];
		if (problems.size() > 1)
			out << "\\end{problem}";
		else
			out << "\\end{problem*}";
		out << "\n\n";
		if (metadata["url"] and !metadata["url"].IsNull())
			out << R"(\noindent\emph{Link}: \url{)" << metadata["url"] << "}"
			    << "\n\n";
		for (size_t i = 2; i < bodies.size(); ++i)
			out << "\\hrulebar" << "\n\n" << bodies[i];
		out << "\n" << "\\pagebreak" << "\n\n";
	}

	out << "\\end{document}" << '\n';

	out.close();
}

void Generate::compile_latex_file(const std::vector<std::string>& problems,
                                  fs::path latex_file_path) {
	if (get<bool>("--no-pdf"))
		return;

	if (!utils::is_executable("latexmk"))
		Log::CRITICAL("latexmk is not executable");
	if (opts.open && !utils::is_executable(opts.pdf_viewer))
		Log::ERROR(opts.pdf_viewer + " is not executable");

	std::vector<std::string> cmd{
	    "latexmk",
	    "-pdf",
	    "-silent",
	};
	if (opts.open) {
		cmd.emplace_back("-pv");
		cmd.emplace_back("-e");
		cmd.emplace_back("'$pdf_previewer=q[" + opts.pdf_viewer + " %S];'");
	}
	fs::path outdir =
	    get<bool>("--cwd") ? fs::current_path() : latex_file_path.parent_path();
	cmd.emplace_back("-outdir=" + outdir.string());
	cmd.emplace_back(latex_file_path.string());
	utils::run(cmd);

	// cleanup
	if (get<bool>("--clean") || get<bool>("--cwd") || get<bool>("--no-source")) {
		std::error_code ec;
		std::vector<std::string> exts{"aux", "fdb_latexmk", "fls", "log", "pre"};
		if (get<bool>("--no-source"))
			exts.emplace_back("tex");

		for (const std::string& ext : exts)
			fs::remove(outdir / latex_file_path.filename().replace_extension(ext), ec);
	}

	if (get<bool>("--print-path")) {
		std::cout << outdir.string() << '\n';
	}
}

void Generate::create_typst_file(const std::vector<std::string>& problems,
                                 const fs::path& typst_file_path) {
	fs::create_directories(typst_file_path.parent_path());
	std::ofstream out(typst_file_path);

	constexpr char TYPST_PREAMBLE[] = {
#embed "../../assets/typst/preamble.typ"
	};
	constexpr size_t LATEX_PREAMBLE_SIZE = sizeof(TYPST_PREAMBLE);
	std::string typst_preamble(TYPST_PREAMBLE, LATEX_PREAMBLE_SIZE);
	shared["packages"] = opts.typst_packages;
	if (problems.size() != 1) {
		out << utils::expand_vars(typst_preamble);
	}

	for (const std::string& problem : problems) {
		const fs::path pb_path = get_problem_solution_path(problem);
		std::vector<std::string> bodies = get_solution_bodies(pb_path);
		YAML::Node metadata_node = get_solution_metadata(pb_path);

		if (problems.size() == 1) {
			metadata = metadata_node;
			out << utils::expand_vars(typst_preamble);
		}

		out << bodies[0]; // packages

		bool is_problem = bodies.size() > 2;
		if (is_problem) {
			if (problems.size() > 1)
				out << "#problem";
			else
				out << "#_problem";
			if (metadata_node["source"])
				out << "(\"" << metadata_node["source"] << "\")";
			out << "[\n";
		}
		if (bodies.size() > 1)
			out << bodies[1];
		if (is_problem)
			out << "]";
		out << "\n\n";

		if (metadata_node["url"] and !metadata_node["url"].IsNull())
			out << "#link(\"" << metadata_node["url"] << "\")[_" << metadata_node["url"]
			    << " _]"
			    << "\n\n";

		for (size_t i = 2; i < bodies.size(); ++i) {
			if (i == 2) {
				out << "#solution[\n" << utils::trim_newlines(bodies[i]) << "\n]";
			} else {
				out << "#divider()" << "\n\n" << bodies[i];
			}
		}

		if (&problem != &problems.back())
			out << "\n" << "#pagebreak()" << "\n\n";
	}

	out.close();
}

void Generate::compile_typst_file(const std::vector<std::string>& problems,
                                  const fs::path& typst_file_path) {
	if (get<bool>("--no-pdf"))
		return;

	if (!utils::is_executable("typst"))
		Log::CRITICAL("typst is not executable");
	if (opts.open && !utils::is_executable(opts.pdf_viewer))
		Log::ERROR(opts.pdf_viewer + " is not executable");

	// BUG: unhandled conflicts (figures with the same name)
	for (std::string problem : problems) {
		utils::figures::copy(typst_file_path.parent_path(), get_problem_path(problem));
	}

	std::vector<std::string> cmd{
	    "typst",
	    "compile",
	    "--root",
	    typst_file_path.parent_path().string(),
	};
	if (opts.open) {
		cmd.emplace_back("--open");
		cmd.emplace_back(opts.pdf_viewer);
	}
	cmd.emplace_back(typst_file_path.string());
	if (get<bool>("--cwd")) {
		fs::path pdf = typst_file_path.filename().replace_extension("pdf");
		cmd.emplace_back((fs::current_path() / pdf).string());
	}
	utils::run(cmd);

	if (get<bool>("--no-source")) {
		fs::remove(typst_file_path);
	}

	if (get<bool>("--print-path")) {
		if (get<bool>("--cwd"))
			std::cout << fs::current_path().string() << '\n';
		else
			std::cout << typst_file_path.parent_path().string() << '\n';
	}
}

int Generate::execute() {
	if (get<bool>("--clear-cache")) {
		fs::path path = opts.output_directory;
		while (path.stem().string().contains("${")) {
			path = path.parent_path();
		}
		if (utils::prompt_before_deletion(path)) {
			fs::remove_all(path);
		}
		return 0;
	}

	if (get<bool>("--all")) {
		opts.open = false;
		set("--regen", true);
		for (const auto& entry : fs::recursive_directory_iterator(opts.base_path)) {
			if (entry.is_regular_file() &&
			    entry.path().filename().stem().string() == "solution") {

				if (entry.path().extension() == ".typ") {
					opts.lang = Config::lang::typst;
				} else if (entry.path().extension() == ".tex") {
					opts.lang = Config::lang::latex;
				} else {
					continue;
				}

				YAML::Node metadata = get_solution_metadata(entry.path());
				if (!metadata["source"]) {
					Log::ERROR("No source entry found in " + entry.path().string());
					continue;
				}

				shared["source"] = metadata["source"].as<std::string>();
				Generate::create_pdf({metadata["source"].as<std::string>()});
			}
		}
		return 0;
	}

	if (positional_args.empty()) {
		for (const std::string& problem : utils::prompt_user_for_problems()) {
			positional_args.push_back(problem);
		}
	}

	std::string source;
	for (const std::string& problem : positional_args) {
		source += get_problem_name(problem) + " - ";
	}
	source = source.substr(0, source.length() - 3);
	shared["source"] = source;

	Generate::create_pdf(positional_args);

	return 0;
}
