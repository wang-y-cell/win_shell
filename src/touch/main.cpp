#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <cstdlib>
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

#ifdef _WIN32
bool parse_touch_time(const std::string& spec, FILETIME& out) {
    // [[CC]YY]MMDDhhmm[.ss]
    std::string body = spec;
    int seconds = 0;
    const auto dot = body.find('.');
    if (dot != std::string::npos) {
        if (dot + 3 != body.size()) {
            return false;
        }
        seconds = std::atoi(body.substr(dot + 1).c_str());
        body = body.substr(0, dot);
    }
    if (body.size() != 8 && body.size() != 10 && body.size() != 12) {
        return false;
    }
    SYSTEMTIME local{};
    GetLocalTime(&local);
    std::size_t i = 0;
    if (body.size() == 12) {
        local.wYear = static_cast<WORD>(std::atoi(body.substr(0, 4).c_str()));
        i = 4;
    } else if (body.size() == 10) {
        local.wYear = static_cast<WORD>(2000 + std::atoi(body.substr(0, 2).c_str()));
        i = 2;
    }
    local.wMonth = static_cast<WORD>(std::atoi(body.substr(i, 2).c_str()));
    local.wDay = static_cast<WORD>(std::atoi(body.substr(i + 2, 2).c_str()));
    local.wHour = static_cast<WORD>(std::atoi(body.substr(i + 4, 2).c_str()));
    local.wMinute = static_cast<WORD>(std::atoi(body.substr(i + 6, 2).c_str()));
    local.wSecond = static_cast<WORD>(seconds);
    local.wMilliseconds = 0;
    SYSTEMTIME utc{};
    if (!TzSpecificLocalTimeToSystemTime(nullptr, &local, &utc)) {
        return false;
    }
    return SystemTimeToFileTime(&utc, &out) != 0;
}

bool read_times(const std::filesystem::path& path, FILETIME& access, FILETIME& write) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) {
        return false;
    }
    access = data.ftLastAccessTime;
    write = data.ftLastWriteTime;
    return true;
}

bool update_times(const std::filesystem::path& path, bool set_access, bool set_write,
                  const FILETIME* access, const FILETIME* write) {
    HANDLE handle =
        CreateFileW(path.c_str(), FILE_WRITE_ATTRIBUTES, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                    OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    FILETIME now{};
    GetSystemTimeAsFileTime(&now);
    const FILETIME* at = set_access ? (access ? access : &now) : nullptr;
    const FILETIME* mt = set_write ? (write ? write : &now) : nullptr;
    const BOOL ok = SetFileTime(handle, nullptr, at, mt);
    CloseHandle(handle);
    return ok != 0;
}
#else
bool update_times(const std::filesystem::path& path, bool, bool, const void*, const void*) {
    std::error_code ec;
    std::filesystem::last_write_time(path, std::filesystem::file_time_type::clock::now(), ec);
    return !ec;
}
#endif

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("touch", "Change file timestamps");
    parser.flag("c", "no-create", "do not create any files")
        .flag("a", "time-access", "change only the access time")
        .flag("m", "time-modify", "change only the modification time")
        .option("r", "reference", "FILE", "use this file's times instead of the current time")
        .option("t", "time", "STAMP", "use [[CC]YY]MMDDhhmm[.ss] instead of the current time")
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
    bool set_access = parsed.has("time-access");
    bool set_write = parsed.has("time-modify");
    if (!set_access && !set_write) {
        set_access = set_write = true;
    }
#ifdef _WIN32
    FILETIME stamp_a{};
    FILETIME stamp_w{};
    const FILETIME* use_a = nullptr;
    const FILETIME* use_w = nullptr;
    if (parsed.has("reference")) {
        const auto ref = utils::sys::path_from_utf8(parsed.get("reference"));
        if (!read_times(ref, stamp_a, stamp_w)) {
            utils::output::writeln_err("touch: failed to get attributes of '" +
                                       parsed.get("reference") + "'");
            return 1;
        }
        use_a = &stamp_a;
        use_w = &stamp_w;
    } else if (parsed.has("time")) {
        if (!parse_touch_time(parsed.get("time"), stamp_w)) {
            utils::output::writeln_err("touch: invalid date format '" + parsed.get("time") + "'");
            return 1;
        }
        stamp_a = stamp_w;
        use_a = &stamp_a;
        use_w = &stamp_w;
    }
#else
    const void* use_a = nullptr;
    const void* use_w = nullptr;
#endif
    auto files = utils::fsutil::expand_globs(parsed.positionals);
    if (files.empty()) {
        utils::output::writeln_err("touch: missing file operand");
        return 1;
    }

    bool had_error = false;
    for (const auto& file : files) {
        const auto path = utils::sys::path_from_utf8(file);
        if (utils::fsutil::exists(path)) {
            if (!update_times(path, set_access, set_write, use_a, use_w)) {
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
            continue;
        }
        out.close();
        if ((use_a != nullptr || use_w != nullptr) &&
            !update_times(path, set_access, set_write, use_a, use_w)) {
            utils::output::writeln_err("touch: cannot touch '" + file + "'");
            had_error = true;
        }
    }
    return had_error ? 1 : 0;
}
