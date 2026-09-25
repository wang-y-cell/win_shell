#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <algorithm>
#include <cstdlib>
#include <fstream>

namespace {

std::string suffix_of(int index, int width) {
    std::string s;
    for (int i = 0; i < width; ++i) {
        s.push_back(static_cast<char>('a' + (index % 26)));
        index /= 26;
    }
    std::reverse(s.begin(), s.end());
    return s;
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("split", "Split a file into pieces");
    parser.option("l", "lines", "N", "put N lines per output file")
        .option("b", "bytes", "N", "put N bytes per output file")
        .option("a", "suffix-length", "N", "use suffixes of length N")
        .flag("", "help", "show this help")
        .positional("FILE", "file to split")
        .positional("PREFIX", "output file prefix");

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

    int lines = parsed.has("lines") ? parsed.get_int("lines", 1000) : 1000;
    int bytes = parsed.has("bytes") ? parsed.get_int("bytes", 0) : 0;
    int width = parsed.has("suffix-length") ? parsed.get_int("suffix-length", 2) : 2;
    if (width < 1) {
        width = 2;
    }

    std::string data;
    std::vector<std::string> text_lines;
    std::string prefix = "x";
    if (parsed.positionals.empty() || parsed.positionals[0] == "-") {
        for (const auto& line : utils::sys::read_stdin_lines()) {
            text_lines.push_back(line);
            data += line;
            data += '\n';
        }
    } else {
        std::string err;
        if (bytes > 0) {
            if (!utils::sys::read_file_bytes(utils::sys::path_from_utf8(parsed.positionals[0]), data,
                                             err)) {
                utils::output::writeln_err("split: " + err);
                return 1;
            }
        } else if (!utils::sys::read_file_lines(utils::sys::path_from_utf8(parsed.positionals[0]),
                                               text_lines, err)) {
            utils::output::writeln_err("split: " + err);
            return 1;
        }
        if (parsed.positionals.size() > 1) {
            prefix = parsed.positionals[1];
        }
    }

    int part = 0;
    if (bytes > 0) {
        for (std::size_t i = 0; i < data.size(); i += static_cast<std::size_t>(bytes), ++part) {
            const auto n = std::min(static_cast<std::size_t>(bytes), data.size() - i);
            const auto path = utils::sys::path_from_utf8(prefix + suffix_of(part, width));
            std::ofstream out(path, std::ios::binary);
            out.write(data.data() + i, static_cast<std::streamsize>(n));
        }
    } else {
        if (lines < 1) {
            lines = 1000;
        }
        for (std::size_t i = 0; i < text_lines.size(); i += static_cast<std::size_t>(lines), ++part) {
            const auto path = utils::sys::path_from_utf8(prefix + suffix_of(part, width));
            std::ofstream out(path, std::ios::binary);
            const std::size_t end = std::min(i + static_cast<std::size_t>(lines), text_lines.size());
            for (std::size_t k = i; k < end; ++k) {
                out << text_lines[k] << '\n';
            }
        }
    }
    return 0;
}
