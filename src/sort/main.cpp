#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace {

double leading_number(const std::string& line) {
    std::size_t i = 0;
    while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
        ++i;
    }
    if (i >= line.size()) {
        return NAN;
    }
    char* end = nullptr;
    const double value = std::strtod(line.c_str() + i, &end);
    if (end == line.c_str() + i) {
        return NAN;
    }
    return value;
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("sort", "Sort lines of text files");
    parser.flag("r", "reverse", "reverse the result of comparisons")
        .flag("n", "numeric-sort", "compare according to string numerical value")
        .flag("u", "unique", "output only the first of an equal run")
        .flag("", "help", "show this help")
        .positional("FILE", "file to sort", true);

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

    const bool reverse = parsed.has("reverse");
    const bool numeric = parsed.has("numeric-sort");
    const bool unique = parsed.has("unique");
    auto files = utils::fsutil::expand_globs(parsed.positionals);

    std::vector<std::string> lines;
    if (files.empty()) {
        lines = utils::sys::read_stdin_lines();
    } else {
        for (const auto& file : files) {
            const auto path = utils::sys::path_from_utf8(file);
            if (!utils::fsutil::exists(path)) {
                utils::output::writeln_err("sort: open failed: " + file +
                                           ": No such file or directory");
                continue;
            }
            if (utils::fsutil::is_dir(path)) {
                utils::output::writeln_err("sort: read failed: " + file + ": Is a directory");
                continue;
            }
            std::string err;
            if (!utils::sys::read_file_lines(path, lines, err)) {
                utils::output::writeln_err("sort: " + file + ": " + err);
            }
        }
    }

    if (numeric) {
        std::stable_sort(lines.begin(), lines.end(), [](const std::string& a, const std::string& b) {
            const double na = leading_number(a);
            const double nb = leading_number(b);
            const bool a_nan = std::isnan(na);
            const bool b_nan = std::isnan(nb);
            if (a_nan != b_nan) {
                return a_nan;
            }
            if (!a_nan && na != nb) {
                return na < nb;
            }
            return a < b;
        });
    } else {
        std::sort(lines.begin(), lines.end());
    }
    if (reverse) {
        std::reverse(lines.begin(), lines.end());
    }

    std::string prev;
    bool has_prev = false;
    for (const auto& line : lines) {
        if (unique && has_prev && line == prev) {
            continue;
        }
        utils::output::writeln(line);
        prev = line;
        has_prev = true;
    }
    return 0;
}
