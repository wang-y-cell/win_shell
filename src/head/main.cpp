#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace {

bool parse_count(const std::string& text, int& value, std::string& error) {
    if (text.empty()) {
        error = "head: invalid number: ''";
        return false;
    }
    char* end = nullptr;
    const long n = std::strtol(text.c_str(), &end, 10);
    if (end == text.c_str() || *end != '\0' || n < 0) {
        error = "head: invalid number: '" + text + "'";
        return false;
    }
    value = static_cast<int>(n);
    return true;
}

void print_first_lines(const std::vector<std::string>& lines, int count) {
    const int n = std::min(count, static_cast<int>(lines.size()));
    for (int i = 0; i < n; ++i) {
        utils::output::writeln(lines[static_cast<std::size_t>(i)]);
    }
}

void print_first_bytes(const std::string& data, int count) {
    if (count <= 0 || data.empty()) {
        return;
    }
    const std::size_t n = std::min(static_cast<std::size_t>(count), data.size());
    std::string chunk = data.substr(0, n);
    if (!chunk.empty() && chunk.back() == '\n') {
        chunk.pop_back();
        if (!chunk.empty() && chunk.back() == '\r') {
            chunk.pop_back();
        }
        utils::output::writeln(chunk);
        return;
    }
    utils::output::write(chunk);
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    auto args = utils::sys::utf8_argv(argc, argv);

    int line_count = 10;
    bool use_bytes = false;
    int byte_count = 0;
    std::vector<std::string> files;
    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string& tok = args[i];
        if (tok.size() >= 2 && tok[0] == '-' && tok[1] >= '0' && tok[1] <= '9') {
            std::string err;
            if (!parse_count(tok.substr(1), line_count, err)) {
                utils::output::writeln_err(err);
                return 1;
            }
            use_bytes = false;
            continue;
        }
        if (tok.rfind("-n", 0) == 0 && tok.size() > 2 && tok[2] >= '0' && tok[2] <= '9') {
            std::string err;
            if (!parse_count(tok.substr(2), line_count, err)) {
                utils::output::writeln_err(err);
                return 1;
            }
            use_bytes = false;
            continue;
        }
        if (tok.rfind("-c", 0) == 0 && tok.size() > 2 && tok[2] >= '0' && tok[2] <= '9') {
            std::string err;
            if (!parse_count(tok.substr(2), byte_count, err)) {
                utils::output::writeln_err(err);
                return 1;
            }
            use_bytes = true;
            continue;
        }
        files.push_back(tok);
    }

    utils::Parser parser("head", "Output the first part of files");
    parser.option("n", "lines", "N", "print the first N lines")
        .option("c", "bytes", "N", "print the first N bytes")
        .flag("", "help", "show this help")
        .positional("FILE", "file to read", true);
    const auto parsed = parser.parse(args);
    if (parsed.has("help")) {
        utils::output::write(parser.help());
        return 0;
    }
    if (parsed.ok) {
        files = parsed.positionals;
        if (parsed.has("lines")) {
            std::string err;
            if (!parse_count(parsed.get("lines"), line_count, err)) {
                utils::output::writeln_err(err);
                return 1;
            }
            use_bytes = false;
        }
        if (parsed.has("bytes")) {
            std::string err;
            if (!parse_count(parsed.get("bytes"), byte_count, err)) {
                utils::output::writeln_err(err);
                return 1;
            }
            use_bytes = true;
        }
    } else if (files.empty()) {
        utils::output::writeln_err(parsed.error);
        return 1;
    }

    files = utils::fsutil::expand_globs(files);
    const bool multi = files.size() > 1;
    if (files.empty()) {
        if (use_bytes) {
            std::ostringstream joined;
            for (const auto& line : utils::sys::read_stdin_lines()) {
                joined << line << '\n';
            }
            print_first_bytes(joined.str(), byte_count);
        } else {
            print_first_lines(utils::sys::read_stdin_lines(), line_count);
        }
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
        if (use_bytes) {
            std::string data;
            std::string err;
            if (!utils::sys::read_file_bytes(path, data, err)) {
                utils::output::writeln_err("head: '" + file + "': " + err);
                had_error = true;
                continue;
            }
            print_first_bytes(data, byte_count);
        } else {
            std::vector<std::string> lines;
            std::string err;
            if (!utils::sys::read_file_lines(path, lines, err)) {
                utils::output::writeln_err("head: '" + file + "': " + err);
                had_error = true;
                continue;
            }
            print_first_lines(lines, line_count);
        }
    }
    return had_error ? 1 : 0;
}
