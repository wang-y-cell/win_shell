#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <algorithm>

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("rev", "Reverse lines characterwise");
    parser.flag("", "help", "show this help").positional("FILE", "file to reverse", true);

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

    auto dump = [](const std::vector<std::string>& lines) {
        for (auto line : lines) {
            std::reverse(line.begin(), line.end());
            utils::output::writeln(line);
        }
    };

    auto files = utils::fsutil::expand_globs(parsed.positionals);
    if (files.empty()) {
        dump(utils::sys::read_stdin_lines());
        return 0;
    }
    bool had_error = false;
    for (const auto& file : files) {
        std::vector<std::string> lines;
        std::string err;
        if (!utils::sys::read_file_lines(utils::sys::path_from_utf8(file), lines, err)) {
            utils::output::writeln_err("rev: " + file + ": " + err);
            had_error = true;
            continue;
        }
        dump(lines);
    }
    return had_error ? 1 : 0;
}
