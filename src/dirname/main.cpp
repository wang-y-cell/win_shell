#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

namespace {

std::string dirname_of(std::string name) {
    if (name.empty()) {
        return ".";
    }
    while (name.size() > 1 && (name.back() == '\\' || name.back() == '/')) {
        name.pop_back();
    }
    if (name.size() == 2 && name[1] == ':') {
        return name + "\\";
    }
    const auto slash = name.find_last_of("\\/");
    if (slash == std::string::npos) {
        return ".";
    }
    if (slash == 0) {
        return name.substr(0, 1);
    }
    if (slash == 2 && name[1] == ':') {
        return name.substr(0, 3);
    }
    return name.substr(0, slash);
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("dirname", "Strip last component from file name");
    parser.flag("", "help", "show this help").positional("NAME", "path name", true);

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
    auto names = utils::fsutil::expand_globs(parsed.positionals);
    if (names.empty()) {
        utils::output::writeln_err("dirname: missing operand");
        return 1;
    }
    for (const auto& name : names) {
        utils::output::writeln(dirname_of(name));
    }
    return 0;
}
