#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <iomanip>
#include <sstream>

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("nl", "Number lines of files");
    parser.option("b", "body-numbering", "STYLE", "a=all, t=nonempty (default t)")
        .option("n", "number-format", "FORMAT", "ln, rn, rz")
        .option("s", "number-separator", "STRING", "add STRING after number")
        .option("w", "number-width", "N", "width of numbers")
        .option("v", "starting-line-number", "N", "first line number")
        .option("i", "line-increment", "N", "increment")
        .flag("", "help", "show this help")
        .positional("FILE", "file to number", true);

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

    const std::string style = parsed.has("body-numbering") ? parsed.get("body-numbering") : "t";
    const std::string fmt = parsed.has("number-format") ? parsed.get("number-format") : "rn";
    const std::string sep = parsed.has("number-separator") ? parsed.get("number-separator") : "\t";
    const int width = parsed.has("number-width") ? parsed.get_int("number-width", 6) : 6;
    int number = parsed.has("starting-line-number") ? parsed.get_int("starting-line-number", 1) : 1;
    const int inc = parsed.has("line-increment") ? parsed.get_int("line-increment", 1) : 1;

    auto emit = [&](const std::vector<std::string>& lines) {
        for (const auto& line : lines) {
            const bool nonempty = line.find_first_not_of(" \t") != std::string::npos;
            const bool number_it = (style == "a") || (style == "t" && nonempty);
            if (!number_it) {
                utils::output::writeln(std::string(static_cast<std::size_t>(width), ' ') + sep + line);
                continue;
            }
            std::ostringstream out;
            if (fmt == "ln") {
                out << std::left << std::setw(width) << number;
            } else if (fmt == "rz") {
                out << std::right << std::setfill('0') << std::setw(width) << number;
            } else {
                out << std::right << std::setw(width) << number;
            }
            utils::output::writeln(out.str() + sep + line);
            number += inc;
        }
    };

    auto files = utils::fsutil::expand_globs(parsed.positionals);
    if (files.empty()) {
        emit(utils::sys::read_stdin_lines());
        return 0;
    }
    bool had_error = false;
    for (const auto& file : files) {
        std::vector<std::string> lines;
        std::string err;
        if (!utils::sys::read_file_lines(utils::sys::path_from_utf8(file), lines, err)) {
            utils::output::writeln_err("nl: " + file + ": " + err);
            had_error = true;
            continue;
        }
        emit(lines);
    }
    return had_error ? 1 : 0;
}
