#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <filesystem>
#include <system_error>

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("install", "Copy files and set attributes");
    parser.flag("d", "directory", "create directories")
        .option("t", "target-directory", "DIR", "copy all SOURCE arguments into DIR")
        .flag("", "help", "show this help")
        .positional("FILE", "source and destination", true);

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

    auto paths = utils::fsutil::expand_globs(parsed.positionals);
    if (parsed.has("directory")) {
        if (paths.empty()) {
            utils::output::writeln_err("install: missing directory operand");
            return 1;
        }
        bool had_error = false;
        for (const auto& dir : paths) {
            std::error_code ec;
            std::filesystem::create_directories(utils::sys::path_from_utf8(dir), ec);
            if (ec) {
                utils::output::writeln_err("install: cannot create directory '" + dir + "': " +
                                           ec.message());
                had_error = true;
            }
        }
        return had_error ? 1 : 0;
    }

    std::filesystem::path dest;
    if (parsed.has("target-directory")) {
        dest = utils::sys::path_from_utf8(parsed.get("target-directory"));
    } else {
        if (paths.size() < 2) {
            utils::output::writeln_err("install: missing destination");
            return 1;
        }
        dest = utils::sys::path_from_utf8(paths.back());
        paths.pop_back();
    }
    if (paths.size() > 1 && !utils::fsutil::is_dir(dest) && !parsed.has("target-directory")) {
        utils::output::writeln_err("install: target is not a directory");
        return 1;
    }

    bool had_error = false;
    for (const auto& src_raw : paths) {
        const auto src = utils::sys::path_from_utf8(src_raw);
        if (!utils::fsutil::exists(src)) {
            utils::output::writeln_err("install: cannot stat '" + src_raw + "'");
            had_error = true;
            continue;
        }
        auto target = dest;
        if (utils::fsutil::is_dir(dest)) {
            target = dest / src.filename();
        }
        std::error_code ec;
        std::filesystem::copy_file(src, target, std::filesystem::copy_options::overwrite_existing, ec);
        if (ec) {
            utils::output::writeln_err("install: cannot install '" + src_raw + "': " + ec.message());
            had_error = true;
        }
    }
    return had_error ? 1 : 0;
}
