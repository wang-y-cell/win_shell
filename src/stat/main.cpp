#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <cstdint>
#include <cstdio>
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

#ifdef _WIN32
std::string format_ft(const FILETIME& ft) {
    SYSTEMTIME utc{};
    SYSTEMTIME local{};
    if (!FileTimeToSystemTime(&ft, &utc)) {
        return "0000-00-00 00:00:00";
    }
    if (!SystemTimeToTzSpecificLocalTime(nullptr, &utc, &local)) {
        local = utc;
    }
    char buf[32];
    std::snprintf(buf, sizeof(buf), " %04u-%02u-%02u %02u:%02u:%02u", local.wYear, local.wMonth,
                  local.wDay, local.wHour, local.wMinute, local.wSecond);
    return buf + 1;
}
#endif

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("stat", "Display file status");
    parser.flag("L", "dereference", "follow links")
        .flag("", "help", "show this help")
        .positional("FILE", "file to inspect", true);

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
        utils::output::writeln_err("stat: missing operand");
        return 1;
    }

    bool had_error = false;
    for (const auto& file : files) {
        const auto path = utils::sys::path_from_utf8(file);
#ifdef _WIN32
        DWORD flags = FILE_FLAG_BACKUP_SEMANTICS;
        if (!parsed.has("dereference")) {
            flags |= FILE_FLAG_OPEN_REPARSE_POINT;
        }
        HANDLE handle = CreateFileW(path.c_str(), 0, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr,
                                    OPEN_EXISTING, flags, nullptr);
        if (handle == INVALID_HANDLE_VALUE) {
            utils::output::writeln_err("stat: cannot stat '" + file + "'");
            had_error = true;
            continue;
        }
        BY_HANDLE_FILE_INFORMATION info{};
        if (!GetFileInformationByHandle(handle, &info)) {
            CloseHandle(handle);
            utils::output::writeln_err("stat: cannot stat '" + file + "'");
            had_error = true;
            continue;
        }
        CloseHandle(handle);
        ULARGE_INTEGER size{};
        size.LowPart = info.nFileSizeLow;
        size.HighPart = info.nFileSizeHigh;
        const bool is_dir = (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        const bool is_link = (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
        utils::output::writeln("  File: " + file);
        utils::output::writeln(std::string("  Type: ") + (is_dir ? "directory" : is_link ? "link" : "file"));
        utils::output::writeln("  Size: " + std::to_string(size.QuadPart));
        utils::output::writeln("Access: " + format_ft(info.ftLastAccessTime));
        utils::output::writeln("Modify: " + format_ft(info.ftLastWriteTime));
        utils::output::writeln(" Birth: " + format_ft(info.ftCreationTime));
#else
        std::error_code ec;
        const auto sz = std::filesystem::file_size(path, ec);
        utils::output::writeln("  File: " + file);
        utils::output::writeln("  Size: " + (ec ? std::string("-") : std::to_string(sz)));
#endif
    }
    return had_error ? 1 : 0;
}
