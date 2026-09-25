#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <sstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

bool parse_tail_count(const std::string& text, int& value, bool& from_start, std::string& error) {
    if (text.empty()) {
        error = "tail: invalid number: ''";
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
        error = "tail: invalid number: '" + text + "'";
        return false;
    }
    value = static_cast<int>(n);
    return true;
}

void print_tail_lines(const std::vector<std::string>& lines, int count, bool from_start) {
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

void print_tail_bytes(const std::string& data, int count, bool from_start) {
    if (data.empty() || count <= 0) {
        return;
    }
    std::string chunk;
    if (from_start) {
        const std::size_t start = std::min(static_cast<std::size_t>(std::max(0, count - 1)), data.size());
        chunk = data.substr(start);
    } else {
        const std::size_t n = std::min(static_cast<std::size_t>(count), data.size());
        chunk = data.substr(data.size() - n);
    }
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

#ifdef _WIN32
bool follow_file(const std::filesystem::path& path, std::uint64_t offset) {
    while (true) {
        Sleep(400);
        std::ifstream in;
        in.open(path, std::ios::binary);
        if (!in) {
            continue;
        }
        in.seekg(0, std::ios::end);
        const auto end = in.tellg();
        if (end < 0 || static_cast<std::uint64_t>(end) <= offset) {
            continue;
        }
        in.seekg(static_cast<std::streamoff>(offset));
        std::string extra(static_cast<std::size_t>(end) - static_cast<std::size_t>(offset), '\0');
        if (!in.read(extra.data(), static_cast<std::streamsize>(extra.size()))) {
            continue;
        }
        offset = static_cast<std::uint64_t>(end);
        utils::output::write(extra);
        utils::output::flush();
    }
}
#endif

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    auto args = utils::sys::utf8_argv(argc, argv);

    int line_count = 10;
    bool from_start = false;
    bool use_bytes = false;
    int byte_count = 0;
    bool follow = false;
    std::vector<std::string> files;
    for (std::size_t i = 1; i < args.size(); ++i) {
        const std::string& tok = args[i];
        if (tok == "-f" || tok == "--follow") {
            follow = true;
            continue;
        }
        if (tok.size() >= 2 && tok[0] == '-' && tok[1] >= '0' && tok[1] <= '9') {
            std::string err;
            bool dummy = false;
            if (!parse_tail_count(tok.substr(1), line_count, dummy, err)) {
                utils::output::writeln_err(err);
                return 1;
            }
            from_start = false;
            use_bytes = false;
            continue;
        }
        if (tok.rfind("-n", 0) == 0 && tok.size() > 2) {
            std::string err;
            if (!parse_tail_count(tok.substr(2), line_count, from_start, err)) {
                utils::output::writeln_err(err);
                return 1;
            }
            use_bytes = false;
            continue;
        }
        if (tok.rfind("-c", 0) == 0 && tok.size() > 2) {
            std::string err;
            if (!parse_tail_count(tok.substr(2), byte_count, from_start, err)) {
                utils::output::writeln_err(err);
                return 1;
            }
            use_bytes = true;
            continue;
        }
        files.push_back(tok);
    }

    utils::Parser parser("tail", "Output the last part of files");
    parser.option("n", "lines", "N", "output the last N lines, or from +N")
        .option("c", "bytes", "N", "output the last N bytes")
        .flag("f", "follow", "output appended data as the file grows")
        .flag("", "help", "show this help")
        .positional("FILE", "file to read", true);
    const auto parsed = parser.parse(args);
    if (parsed.has("help")) {
        utils::output::write(parser.help());
        return 0;
    }
    if (parsed.ok) {
        files = parsed.positionals;
        follow = follow || parsed.has("follow");
        if (parsed.has("lines")) {
            std::string err;
            if (!parse_tail_count(parsed.get("lines"), line_count, from_start, err)) {
                utils::output::writeln_err(err);
                return 1;
            }
            use_bytes = false;
        }
        if (parsed.has("bytes")) {
            std::string err;
            if (!parse_tail_count(parsed.get("bytes"), byte_count, from_start, err)) {
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
            print_tail_bytes(joined.str(), byte_count, from_start);
        } else {
            print_tail_lines(utils::sys::read_stdin_lines(), line_count, from_start);
        }
        return 0;
    }

    bool had_error = false;
    std::filesystem::path follow_path;
    std::uint64_t follow_off = 0;
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
        std::string data;
        std::string err;
        if (!utils::sys::read_file_bytes(path, data, err)) {
            utils::output::writeln_err("tail: '" + file + "': " + err);
            had_error = true;
            continue;
        }
        if (use_bytes) {
            print_tail_bytes(data, byte_count, from_start);
        } else {
            std::vector<std::string> lines;
            if (!utils::sys::read_file_lines(path, lines, err)) {
                utils::output::writeln_err("tail: '" + file + "': " + err);
                had_error = true;
                continue;
            }
            print_tail_lines(lines, line_count, from_start);
        }
        follow_path = path;
        follow_off = data.size();
    }
#ifdef _WIN32
    if (follow && !follow_path.empty() && !had_error) {
        follow_file(follow_path, follow_off);
    }
#else
    (void)follow;
    (void)follow_off;
#endif
    return had_error ? 1 : 0;
}
