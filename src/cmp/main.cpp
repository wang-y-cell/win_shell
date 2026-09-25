#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <algorithm>

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("cmp", "Compare two files byte by byte");
    parser.flag("l", "verbose", "output byte numbers and differing byte values")
        .flag("s", "quiet", "suppress all normal output")
        .flag("", "help", "show this help")
        .positional("FILE", "two files", true);

    const auto args = utils::sys::utf8_argv(argc, argv);
    const auto parsed = parser.parse(args);
    if (!parsed.ok) {
        utils::output::writeln_err(parsed.error);
        return 2;
    }
    if (parsed.has("help")) {
        utils::output::write(parser.help());
        return 0;
    }
    auto files = utils::fsutil::expand_globs(parsed.positionals);
    if (files.size() != 2) {
        utils::output::writeln_err("cmp: two files required");
        return 2;
    }

    std::string a;
    std::string b;
    std::string err;
    if (!utils::sys::read_file_bytes(utils::sys::path_from_utf8(files[0]), a, err) ||
        !utils::sys::read_file_bytes(utils::sys::path_from_utf8(files[1]), b, err)) {
        utils::output::writeln_err("cmp: " + err);
        return 2;
    }

    const bool quiet = parsed.has("quiet");
    const bool verbose = parsed.has("verbose");
    const std::size_t n = std::min(a.size(), b.size());
    bool differ = a.size() != b.size();
    std::size_t first = static_cast<std::size_t>(-1);
    for (std::size_t i = 0; i < n; ++i) {
        if (a[i] != b[i]) {
            differ = true;
            if (first == static_cast<std::size_t>(-1)) {
                first = i;
            }
            if (verbose && !quiet) {
                utils::output::writeln(std::to_string(i + 1) + " " +
                                       std::to_string(static_cast<unsigned char>(a[i])) + " " +
                                       std::to_string(static_cast<unsigned char>(b[i])));
            } else if (!verbose) {
                break;
            }
        }
    }
    if (!differ) {
        return 0;
    }
    if (!quiet && !verbose) {
        int line = 1;
        const std::size_t pos = first == static_cast<std::size_t>(-1) ? n : first;
        for (std::size_t i = 0; i < pos && i < a.size(); ++i) {
            if (a[i] == '\n') {
                ++line;
            }
        }
        utils::output::writeln(files[0] + " " + files[1] + " differ: byte " +
                               std::to_string(pos + 1) + ", line " + std::to_string(line));
    }
    return 1;
}
