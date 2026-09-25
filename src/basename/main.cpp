#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

namespace {

std::string basename_of(std::string name, const std::string& suffix) {
    while (name.size() > 1 && (name.back() == '\\' || name.back() == '/')) {
        name.pop_back();
    }
    if (name.size() == 2 && name[1] == ':') {
        return name + "\\";
    }
    const auto slash = name.find_last_of("\\/");
    std::string base = slash == std::string::npos ? name : name.substr(slash + 1);
    if (!suffix.empty() && base.size() > suffix.size() &&
        base.compare(base.size() - suffix.size(), suffix.size(), suffix) == 0) {
        base.resize(base.size() - suffix.size());
    }
    return base;
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("basename", "Strip directory and suffix from filenames");
    parser.flag("a", "multiple", "support multiple arguments")
        .option("s", "suffix", "SUFFIX", "remove a trailing suffix")
        .flag("", "help", "show this help")
        .positional("NAME", "path name", true);

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

    auto names = parsed.positionals;
    if (names.empty()) {
        utils::output::writeln_err("basename: missing operand");
        return 1;
    }

    const bool all = parsed.has("multiple") || parsed.has("suffix");
    std::string suffix = parsed.get("suffix");
    if (!all) {
        if (names.size() > 2) {
            utils::output::writeln_err("basename: extra operand (use -a for multiple names)");
            return 1;
        }
        if (names.size() == 2) {
            suffix = names[1];
            names.resize(1);
        }
    }

    names = utils::fsutil::expand_globs(names);
    for (const auto& name : names) {
        utils::output::writeln(basename_of(name, suffix));
    }
    return 0;
}
