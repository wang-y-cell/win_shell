#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <algorithm>

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("paste", "Merge lines of files");
    parser.option("d", "delimiters", "LIST", "reuse characters from LIST instead of TABs")
        .flag("s", "serial", "paste one file at a time instead of in parallel")
        .flag("", "help", "show this help")
        .positional("FILE", "file to paste, or - for stdin", true);

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

    std::string delims = parsed.has("delimiters") ? parsed.get("delimiters") : "\t";
    if (delims.empty()) {
        delims = "\t";
    }
    auto files = parsed.positionals.empty() ? std::vector<std::string>{"-"} : parsed.positionals;
    files = utils::fsutil::expand_globs(files);

    std::vector<std::vector<std::string>> cols;
    for (const auto& file : files) {
        if (file == "-") {
            cols.push_back(utils::sys::read_stdin_lines());
        } else {
            std::vector<std::string> lines;
            std::string err;
            if (!utils::sys::read_file_lines(utils::sys::path_from_utf8(file), lines, err)) {
                utils::output::writeln_err("paste: " + file + ": " + err);
                return 1;
            }
            cols.push_back(std::move(lines));
        }
    }

    if (parsed.has("serial")) {
        for (const auto& col : cols) {
            for (std::size_t i = 0; i < col.size(); ++i) {
                utils::output::write(col[i]);
                if (i + 1 < col.size()) {
                    utils::output::write(std::string(1, delims[i % delims.size()]));
                }
            }
            utils::output::writeln("");
        }
        return 0;
    }

    std::size_t rows = 0;
    for (const auto& col : cols) {
        rows = std::max(rows, col.size());
    }
    for (std::size_t r = 0; r < rows; ++r) {
        for (std::size_t c = 0; c < cols.size(); ++c) {
            if (c) {
                utils::output::write(std::string(1, delims[(c - 1) % delims.size()]));
            }
            if (r < cols[c].size()) {
                utils::output::write(cols[c][r]);
            }
        }
        utils::output::writeln("");
    }
    return 0;
}
