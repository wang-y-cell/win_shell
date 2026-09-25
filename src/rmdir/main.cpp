#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <filesystem>
#include <system_error>

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("rmdir", "Remove empty directories");
    parser.flag("p", "parents", "remove DIRECTORY and its ancestors")
        .flag("", "help", "show this help")
        .positional("DIR", "empty directory", true);

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
    auto dirs = utils::fsutil::expand_globs(parsed.positionals);
    if (dirs.empty()) {
        utils::output::writeln_err("rmdir: missing operand");
        return 1;
    }

    bool had_error = false;
    for (const auto& dir : dirs) {
        auto path = utils::sys::path_from_utf8(dir);
        std::error_code ec;
        do {
            if (!std::filesystem::remove(path, ec) || ec) {
                utils::output::writeln_err("rmdir: failed to remove '" + dir + "': " +
                                           (ec ? ec.message() : std::string("Directory not empty")));
                had_error = true;
                break;
            }
            path = path.parent_path();
        } while (parsed.has("parents") && !path.empty() && path != path.root_path());
    }
    return had_error ? 1 : 0;
}
