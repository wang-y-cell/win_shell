#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <filesystem>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

void clear_readonly(const std::filesystem::path& path) {
#ifdef _WIN32
    const DWORD attr = GetFileAttributesW(path.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_READONLY)) {
        SetFileAttributesW(path.c_str(), attr & ~FILE_ATTRIBUTE_READONLY);
    }
#else
    (void)path;
#endif
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("rm", "Remove files or directories");
    parser.flag("r", "recursive", "remove directories and their contents recursively")
        .flag("R", "RECURSIVE", "same as -r")
        .flag("f", "force", "ignore nonexistent files, never prompt")
        .flag("i", "interactive", "prompt before every removal")
        .flag("v", "verbose", "explain what is being done")
        .flag("", "help", "show this help")
        .positional("FILE", "file or directory to remove", true);

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
    const bool interactive = parsed.has("interactive") && !force;
    const bool verbose = parsed.has("verbose");
    auto paths = utils::fsutil::expand_globs(parsed.positionals);
    if (paths.empty()) {
        utils::output::writeln_err("rm: missing operand");
        return 1;
    }

    bool had_error = false;
    for (const auto& raw : paths) {
        const auto path = utils::sys::path_from_utf8(raw);
        if (!utils::fsutil::exists(path)) {
            if (!force) {
                utils::output::writeln_err("rm: cannot remove '" + raw +
                                           "': No such file or directory");
                had_error = true;
            }
            continue;
        }
        if (utils::fsutil::is_dir(path) && !recursive) {
            utils::output::writeln_err("rm: cannot remove '" + raw + "': Is a directory");
            had_error = true;
            continue;
        }
        if (interactive && !utils::sys::confirm("rm: remove '" + raw + "'? ")) {
            continue;
        }
        std::error_code ec;
        if (force) {
            clear_readonly(path);
        }
        if (recursive) {
            std::filesystem::remove_all(path, ec);
        } else {
            std::filesystem::remove(path, ec);
        }
        if (ec && !force) {
            utils::output::writeln_err("rm: cannot remove '" + raw + "': " + ec.message());
            had_error = true;
        } else if (!ec && verbose) {
            utils::output::writeln("removed '" + raw + "'");
        }
    }
    return had_error ? 1 : 0;
}
