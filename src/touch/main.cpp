#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <fstream>
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

bool update_times(const std::filesystem::path& path) {
#ifdef _WIN32
    HANDLE handle =
        CreateFileW(path.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                    OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    FILETIME now{};
    GetSystemTimeAsFileTime(&now);
    const BOOL ok = SetFileTime(handle, nullptr, &now, &now);
    CloseHandle(handle);
    return ok != 0;
#else
    std::error_code ec;
    std::filesystem::last_write_time(path, std::filesystem::file_time_type::clock::now(), ec);
    return !ec;
#endif
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("touch", "Change file timestamps");
    parser.flag("c", "no-create", "do not create any files")
        .flag("", "help", "show this help")
        .positional("FILE", "file to touch", true);

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

    const bool no_create = parsed.has("no-create");
    auto files = utils::fsutil::expand_globs(parsed.positionals);
    if (files.empty()) {
        utils::output::writeln_err("touch: missing file operand");
        return 1;
    }

    bool had_error = false;
    for (const auto& file : files) {
        const auto path = utils::sys::path_from_utf8(file);
        if (utils::fsutil::exists(path)) {
            if (!update_times(path)) {
                utils::output::writeln_err("touch: cannot touch '" + file + "'");
                had_error = true;
            }
            continue;
        }
        if (no_create) {
            continue;
        }
        const auto parent = path.parent_path();
        if (!parent.empty() && !utils::fsutil::exists(parent)) {
            utils::output::writeln_err("touch: cannot touch '" + file +
                                       "': No such file or directory");
            had_error = true;
            continue;
        }
        std::ofstream out;
        out.open(path, std::ios::binary | std::ios::app);
        if (!out) {
            utils::output::writeln_err("touch: cannot touch '" + file + "'");
            had_error = true;
        }
    }
    return had_error ? 1 : 0;
}
