#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <filesystem>
#include <system_error>

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("cp", "Copy files and directories");
    parser.flag("r", "recursive", "copy directories recursively")
        .flag("R", "RECURSIVE", "same as -r")
        .flag("f", "force", "overwrite destination if it exists")
        .flag("v", "verbose", "explain what is being done")
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

    const bool recursive = parsed.has("recursive") || parsed.has("RECURSIVE");
    const bool force = parsed.has("force");
    const bool verbose = parsed.has("verbose");
    auto paths = utils::fsutil::expand_globs(parsed.positionals);
    if (paths.size() < 2) {
        utils::output::writeln_err("cp: missing file operand");
        return 1;
    }

    const std::string dest_raw = paths.back();
    paths.pop_back();
    const auto dest = utils::sys::path_from_utf8(dest_raw);
    if (paths.size() > 1 && !utils::fsutil::is_dir(dest)) {
        utils::output::writeln_err("cp: target '" + dest_raw + "' is not a directory");
        return 1;
    }

    bool had_error = false;
    for (const auto& src_raw : paths) {
        const auto src = utils::sys::path_from_utf8(src_raw);
        if (!utils::fsutil::exists(src)) {
            utils::output::writeln_err("cp: cannot stat '" + src_raw +
                                       "': No such file or directory");
            had_error = true;
            continue;
        }
        if (utils::fsutil::is_dir(src) && !recursive) {
            utils::output::writeln_err("cp: -r not specified; omitting directory '" + src_raw + "'");
            had_error = true;
            continue;
        }

        auto target = dest;
        if (utils::fsutil::is_dir(dest)) {
            target = dest / src.filename();
        }
        std::error_code ec;
        if (std::filesystem::exists(target, ec) && !force) {
            utils::output::writeln_err("cp: cannot copy '" + src_raw + "' to '" + dest_raw +
                                       "': File exists");
            had_error = true;
            continue;
        }
        const auto options = force ? std::filesystem::copy_options::overwrite_existing
                                   : std::filesystem::copy_options::none;
        if (utils::fsutil::is_dir(src)) {
            std::filesystem::copy(src, target,
                                  options | std::filesystem::copy_options::recursive, ec);
        } else {
            std::filesystem::copy_file(src, target, options, ec);
        }
        if (ec) {
            utils::output::writeln_err("cp: cannot copy '" + src_raw + "' to '" + dest_raw + "': " +
                                       ec.message());
            had_error = true;
            continue;
        }
        if (verbose) {
            utils::output::writeln("'" + src_raw + "' -> '" + dest_raw + "'");
        }
    }
    return had_error ? 1 : 0;
}
