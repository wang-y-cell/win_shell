#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <cctype>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace {

struct Counts {
    std::uint64_t lines = 0;
    std::uint64_t words = 0;
    std::uint64_t bytes = 0;
};

void count_line(const std::string& line, Counts& c, bool add_newline) {
    ++c.lines;
    bool in_word = false;
    for (unsigned char ch : line) {
        if (std::isspace(ch)) {
            in_word = false;
        } else if (!in_word) {
            in_word = true;
            ++c.words;
        }
    }
    c.bytes += line.size() + (add_newline ? 1 : 0);
}

void print_counts(const Counts& c, bool show_l, bool show_w, bool show_c, const std::string& label) {
    std::ostringstream out;
    bool first = true;
    auto field = [&](std::uint64_t value) {
        if (!first) {
            out << ' ';
        }
        first = false;
        out << std::setw(8) << value;
    };
    if (show_l) {
        field(c.lines);
    }
    if (show_w) {
        field(c.words);
    }
    if (show_c) {
        field(c.bytes);
    }
    if (!label.empty()) {
        out << ' ' << label;
    }
    utils::output::writeln(out.str());
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("wc", "Print newline, word, and byte counts");
    parser.flag("l", "lines", "print the newline counts")
        .flag("w", "words", "print the word counts")
        .flag("c", "bytes", "print the byte counts")
        .flag("", "help", "show this help")
        .positional("FILE", "file to count", true);

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

    bool show_l = parsed.has("lines");
    bool show_w = parsed.has("words");
    bool show_c = parsed.has("bytes");
    if (!show_l && !show_w && !show_c) {
        show_l = show_w = show_c = true;
    }

    auto files = utils::fsutil::expand_globs(parsed.positionals);
    if (files.empty()) {
        Counts c;
        for (const auto& line : utils::sys::read_stdin_lines()) {
            count_line(line, c, true);
        }
        print_counts(c, show_l, show_w, show_c, "");
        return 0;
    }

    Counts total;
    int ok = 0;
    bool had_error = false;
    for (const auto& file : files) {
        const auto path = utils::sys::path_from_utf8(file);
        if (!utils::fsutil::exists(path)) {
            utils::output::writeln_err("wc: " + file + ": No such file or directory");
            had_error = true;
            continue;
        }
        if (utils::fsutil::is_dir(path)) {
            utils::output::writeln_err("wc: " + file + ": Is a directory");
            had_error = true;
            continue;
        }
        Counts c;
        std::error_code ec;
        c.bytes = std::filesystem::file_size(path, ec);
        std::vector<std::string> lines;
        std::string err;
        if (!utils::sys::read_file_lines(path, lines, err)) {
            utils::output::writeln_err("wc: " + file + ": " + err);
            had_error = true;
            continue;
        }
        for (const auto& line : lines) {
            ++c.lines;
            bool in_word = false;
            for (unsigned char ch : line) {
                if (std::isspace(ch)) {
                    in_word = false;
                } else if (!in_word) {
                    in_word = true;
                    ++c.words;
                }
            }
        }
        if (ec) {
            c.bytes = 0;
            for (const auto& line : lines) {
                c.bytes += line.size() + 1;
            }
        }
        print_counts(c, show_l, show_w, show_c, file);
        total.lines += c.lines;
        total.words += c.words;
        total.bytes += c.bytes;
        ++ok;
    }
    if (ok > 1) {
        print_counts(total, show_l, show_w, show_c, "total");
    }
    return had_error ? 1 : 0;
}
