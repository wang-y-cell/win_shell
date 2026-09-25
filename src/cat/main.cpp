#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <iomanip>
#include <sstream>

namespace {

void emit(const std::string& line, bool number_all, bool number_nonblank, int& all_no,
          int& nb_no) {
    if (number_nonblank) {
        if (line.find_first_not_of(" \t\r") != std::string::npos) {
            ++nb_no;
            std::ostringstream out;
            out << std::setw(6) << nb_no << '\t' << line;
            utils::output::writeln(out.str());
        } else {
            utils::output::writeln(line);
        }
        return;
    }
    if (number_all) {
        ++all_no;
        std::ostringstream out;
        out << std::setw(6) << all_no << '\t' << line;
        utils::output::writeln(out.str());
        return;
    }
    utils::output::writeln(line);
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("cat", "Concatenate files and print on the standard output");
    parser.flag("n", "number", "number all output lines")
        .flag("b", "number-nonblank", "number nonempty output lines")
        .flag("", "help", "show this help")
        .positional("FILE", "file to print, or - for stdin", true);

    const auto args = utils::sys::utf8_argv(argc, argv);
    const auto parsed = parser.parse(args);
    if (!parsed.ok) {
        utils::output::writeln_err(parsed.error);
        return 1;
    }
    if (parsed.has("help")) {
        utils::output::write(parser.help());
        return 0;
    }

    const bool number_nonblank = parsed.has("number-nonblank");
    const bool number_all = parsed.has("number") && !number_nonblank;
    auto files = utils::fsutil::expand_globs(parsed.positionals);
    int all_no = 0;
    int nb_no = 0;
    bool had_error = false;

    auto dump_lines = [&](const std::vector<std::string>& lines) {
        for (const auto& line : lines) {
            emit(line, number_all, number_nonblank, all_no, nb_no);
        }
    };

    if (files.empty()) {
        dump_lines(utils::sys::read_stdin_lines());
        return 0;
    }

    for (const auto& file : files) {
        if (file == "-") {
            dump_lines(utils::sys::read_stdin_lines());
            continue;
        }
        const auto path = utils::sys::path_from_utf8(file);
        if (!utils::fsutil::exists(path)) {
            utils::output::writeln_err("cat: " + file + ": No such file or directory");
            had_error = true;
            continue;
        }
        if (utils::fsutil::is_dir(path)) {
            utils::output::writeln_err("cat: " + file + ": Is a directory");
            had_error = true;
            continue;
        }
        std::vector<std::string> lines;
        std::string err;
        if (!utils::sys::read_file_lines(path, lines, err)) {
            utils::output::writeln_err("cat: " + file + ": " + err);
            had_error = true;
            continue;
        }
        dump_lines(lines);
    }
    return had_error ? 1 : 0;
}
