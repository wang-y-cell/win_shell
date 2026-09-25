#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <filesystem>
#include <system_error>

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("mkdir", "Create directories");
    parser.flag("p", "parents", "create parent directories as needed")
        .flag("", "help", "show this help")
        .positional("DIR", "directory to create", true);

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

    const bool parents = parsed.has("parents");
    auto dirs = utils::fsutil::expand_globs(parsed.positionals);
    if (dirs.empty()) {
        utils::output::writeln_err("mkdir: missing operand");
        return 1;
    }

    bool had_error = false;
    for (const auto& dir : dirs) {
        const auto path = utils::sys::path_from_utf8(dir);
        std::error_code ec;
        if (std::filesystem::exists(path, ec)) {
            if (!parents) {
                utils::output::writeln_err("mkdir: cannot create directory '" + dir +
                                           "': File exists");
                had_error = true;
            }
            continue;
        }
        if (parents) {
            std::filesystem::create_directories(path, ec);
        } else {
            const auto parent = path.parent_path();
            if (!parent.empty() && !std::filesystem::exists(parent, ec)) {
                utils::output::writeln_err("mkdir: cannot create directory '" + dir +
                                           "': No such file or directory");
                had_error = true;
                continue;
            }
            std::filesystem::create_directory(path, ec);
        }
        if (ec) {
            utils::output::writeln_err("mkdir: cannot create directory '" + dir + "': " +
                                       ec.message());
            had_error = true;
        }
    }
    return had_error ? 1 : 0;
}
