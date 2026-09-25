#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <cstdlib>
#include <deque>

namespace {

bool parse_tail_count(const std::string& text, int& value, bool& from_start, std::string& error) {
    if (text.empty()) {
        error = "tail: invalid number of lines: ''";
        return false;
    }
    std::string body = text;
    from_start = false;
    if (body[0] == '+') {
        from_start = true;
        body = body.substr(1);
    }
    char* end = nullptr;
    const long n = std::strtol(body.c_str(), &end, 10);
    if (body.empty() || end == body.c_str() || *end != '\0' || n < 0) {
        error = "tail: invalid number of lines: '" + text + "'";
        return false;
    }
    value = static_cast<int>(n);
    return true;
}

void print_tail(const std::vector<std::string>& lines, int count, bool from_start) {
    if (from_start) {
        const int start = std::max(1, count);
        for (int i = start - 1; i < static_cast<int>(lines.size()); ++i) {
            utils::output::writeln(lines[static_cast<std::size_t>(i)]);
        }
        return;
    }
    const int n = std::min(count, static_cast<int>(lines.size()));
    const int begin = static_cast<int>(lines.size()) - n;
    for (int i = begin; i < static_cast<int>(lines.size()); ++i) {
        utils::output::writeln(lines[static_cast<std::size_t>(i)]);
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    auto args = utils::sys::utf8_argv(argc, argv);

    int line_count = 10;
    bool from_start = false;
    std::vector<std::string> files;
    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string& tok = args[i];
        if (tok.size() >= 2 && tok[0] == '-' && tok[1] >= '0' && tok[1] <= '9') {
            std::string err;
            bool dummy = false;
            if (!parse_tail_count(tok.substr(1), line_count, dummy, err)) {
                utils::output::writeln_err(err);
                return 1;
            }
            from_start = false;
            continue;
        }
        if (tok.rfind("-n", 0) == 0 && tok.size() > 2) {
            std::string err;
            if (!parse_tail_count(tok.substr(2), line_count, from_start, err)) {
                utils::output::writeln_err(err);
                return 1;
            }
            continue;
        }
        files.push_back(tok);
    }

    utils::Parser parser("tail", "Output the last part of files");
    parser.option("n", "lines", "N", "output the last N lines, or from +N")
        .flag("", "help", "show this help")
        .positional("FILE", "file to read", true);
    const auto parsed = parser.parse(args);
    if (parsed.has("help")) {
        utils::output::write(parser.help());
        return 0;
    }
    if (parsed.ok && parsed.has("lines")) {
        std::string err;
        if (!parse_tail_count(parsed.get("lines"), line_count, from_start, err)) {
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
        print_tail(utils::sys::read_stdin_lines(), line_count, from_start);
        return 0;
    }

    bool had_error = false;
    for (std::size_t i = 0; i < files.size(); ++i) {
        const auto& file = files[i];
        const auto path = utils::sys::path_from_utf8(file);
        if (!utils::fsutil::exists(path)) {
            utils::output::writeln_err("tail: cannot open '" + file +
                                       "' for reading: No such file or directory");
            had_error = true;
            continue;
        }
        if (utils::fsutil::is_dir(path)) {
            utils::output::writeln_err("tail: error reading '" + file + "': Is a directory");
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
            utils::output::writeln_err("tail: '" + file + "': " + err);
            had_error = true;
            continue;
        }
        print_tail(lines, line_count, from_start);
    }
    return had_error ? 1 : 0;
}
