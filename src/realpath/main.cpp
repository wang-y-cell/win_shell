#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <filesystem>
#include <system_error>

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("realpath", "Print the resolved path");
    parser.flag("s", "no-symlinks", "do not expand symbolic links")
        .flag("q", "quiet", "suppress error messages")
        .flag("", "help", "show this help")
        .positional("FILE", "path to resolve", true);

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
    if (files.empty()) {
        utils::output::writeln_err("realpath: missing operand");
        return 1;
    }

    bool had_error = false;
    for (const auto& file : files) {
        const auto path = utils::sys::path_from_utf8(file);
        std::error_code ec;
        std::filesystem::path out =
            parsed.has("no-symlinks") ? std::filesystem::absolute(path, ec)
                                      : std::filesystem::weakly_canonical(path, ec);
        if (ec) {
            if (!parsed.has("quiet")) {
                utils::output::writeln_err("realpath: " + file + ": " + ec.message());
            }
            had_error = true;
            continue;
        }
        utils::output::writeln(utils::sys::path_to_utf8(out));
    }
    return had_error ? 1 : 0;
}
