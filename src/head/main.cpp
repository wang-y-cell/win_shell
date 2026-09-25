#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <cstdlib>

namespace {

bool parse_count(const std::string& text, int& value, std::string& error) {
    if (text.empty()) {
        error = "head: invalid number of lines: ''";
        return false;
    }
    char* end = nullptr;
    const long n = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0' || n < 0) {
        error = "head: invalid number of lines: '" + text + "'";
        return false;
    }
    value = static_cast<int>(n);
    return true;
}

void print_first(const std::vector<std::string>& lines, int count) {
    const int n = std::min(count, static_cast<int>(lines.size()));
    for (int i = 0; i < n; ++i) {
        utils::output::writeln(lines[static_cast<std::size_t>(i)]);
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    auto args = utils::sys::utf8_argv(argc, argv);

    int line_count = 10;
    std::vector<std::string> files;
    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string& tok = args[i];
        if (tok.size() >= 2 && tok[0] == '-' && tok[1] >= '0' && tok[1] <= '9') {
            std::string err;
            if (!parse_count(tok.substr(1), line_count, err)) {
                utils::output::writeln_err(err);
                return 1;
            }
            continue;
        }
        if (tok.rfind("-n", 0) == 0 && tok.size() > 2 && tok[2] >= '0' && tok[2] <= '9') {
            std::string err;
            if (!parse_count(tok.substr(2), line_count, err)) {
                utils::output::writeln_err(err);
                return 1;
            }
            continue;
        }
        files.push_back(tok);
    }

    utils::Parser parser("head", "Output the first part of files");
    parser.option("n", "lines", "N", "print the first N lines")
        .flag("", "help", "show this help")
        .positional("FILE", "file to read", true);
    const auto parsed = parser.parse(args);
    if (parsed.has("help")) {
        utils::output::write(parser.help());
        return 0;
    }
    if (parsed.ok && parsed.has("lines")) {
        std::string err;
        if (!parse_count(parsed.get("lines"), line_count, err)) {
            utils::output::writeln_err(err);
            return 1;
        }
        files = parsed.positionals;
    } else if (!parsed.ok && files.empty()) {
        utils::output::writeln_err(parsed.error);
        return 1;
    }

    files = utils::fsutil::expand_globs(files);
    const bool multi = files.size() > 1;
    if (files.empty()) {
        print_first(utils::sys::read_stdin_lines(), line_count);
        return 0;
    }

    bool had_error = false;
    for (std::size_t i = 0; i < files.size(); ++i) {
        const auto& file = files[i];
        const auto path = utils::sys::path_from_utf8(file);
        if (!utils::fsutil::exists(path)) {
            utils::output::writeln_err("head: cannot open '" + file +
                                       "' for reading: No such file or directory");
            had_error = true;
            continue;
        }
        if (utils::fsutil::is_dir(path)) {
            utils::output::writeln_err("head: error reading '" + file + "': Is a directory");
            had_error = true;
            continue;
        }
        if (multi) {
            if (i > 0) {
                utils::output::writeln("");
            }
            utils::output::writeln("==> " + file + " <==");
        }
        std::vector<std::string> lines;
        std::string err;
        if (!utils::sys::read_file_lines(path, lines, err)) {
            utils::output::writeln_err("head: '" + file + "': " + err);
            had_error = true;
            continue;
        }
        print_first(lines, line_count);
    }
    return had_error ? 1 : 0;
}
