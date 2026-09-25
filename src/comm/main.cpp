#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("comm", "Compare two sorted files line by line");
    parser.flag("1", "suppress-1", "suppress column 1 (lines unique to FILE1)")
        .flag("2", "suppress-2", "suppress column 2 (lines unique to FILE2)")
        .flag("3", "suppress-3", "suppress column 3 (lines common to both)")
        .flag("", "help", "show this help")
        .positional("FILE", "two sorted files", true);

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
    auto files = utils::fsutil::expand_globs(parsed.positionals);
    if (files.size() != 2) {
        utils::output::writeln_err("comm: two files required");
        return 1;
    }

    std::vector<std::string> a;
    std::vector<std::string> b;
    std::string err;
    if (!utils::sys::read_file_lines(utils::sys::path_from_utf8(files[0]), a, err) ||
        !utils::sys::read_file_lines(utils::sys::path_from_utf8(files[1]), b, err)) {
        utils::output::writeln_err("comm: " + err);
        return 1;
    }

    const bool hide1 = parsed.has("suppress-1");
    const bool hide2 = parsed.has("suppress-2");
    const bool hide3 = parsed.has("suppress-3");
    std::size_t i = 0;
    std::size_t j = 0;
    while (i < a.size() || j < b.size()) {
        if (j >= b.size() || (i < a.size() && a[i] < b[j])) {
            if (!hide1) {
                utils::output::writeln(a[i]);
            }
            ++i;
        } else if (i >= a.size() || b[j] < a[i]) {
            if (!hide2) {
                utils::output::writeln((hide1 ? "" : "\t") + b[j]);
            }
            ++j;
        } else {
            if (!hide3) {
                std::string prefix;
                if (!hide1) {
                    prefix += '\t';
                }
                if (!hide2) {
                    prefix += '\t';
                }
                utils::output::writeln(prefix + a[i]);
            }
            ++i;
            ++j;
        }
    }
    return 0;
}
