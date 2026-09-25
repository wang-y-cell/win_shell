#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"
#include "utils/theme.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <sstream>

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

struct Row {
    std::string fs;
    std::uint64_t size = 0;
    std::uint64_t used = 0;
    std::uint64_t avail = 0;
    int pct = 0;
    std::string mounted;
};

#ifdef _WIN32
std::vector<Row> collect_drives() {
    std::vector<Row> rows;
    const DWORD mask = GetLogicalDrives();
    for (int i = 0; i < 26; ++i) {
        if ((mask & (1u << i)) == 0) {
            continue;
        }
        wchar_t root[] = {static_cast<wchar_t>(L'A' + i), L':', L'\\', L'\0'};
        ULARGE_INTEGER free_bytes{}, total{}, free_avail{};
        if (!GetDiskFreeSpaceExW(root, &free_bytes, &total, &free_avail)) {
            continue;
        }
        Row row;
        row.fs = std::string(1, static_cast<char>('A' + i)) + ":";
        row.size = total.QuadPart;
        row.avail = free_bytes.QuadPart;
        row.used = row.size > row.avail ? row.size - row.avail : 0;
        row.pct = row.size > 0 ? static_cast<int>((row.used * 100 + row.size / 2) / row.size) : 0;
        row.mounted = row.fs + "\\";
        rows.push_back(row);
    }
    return rows;
}
#else
std::vector<Row> collect_drives() { return {}; }
#endif

std::string drive_letter(const std::string& path) {
    if (path.size() >= 2 && path[1] == ':') {
        char c = path[0];
        if (c >= 'a' && c <= 'z') {
            c = static_cast<char>(c - 'a' + 'A');
        }
        return std::string(1, c);
    }
    return {};
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("df", "Report file system disk space usage");
    parser.flag("h", "human-readable", "print sizes in human readable format")
        .flag("", "help", "show this help")
        .positional("PATH", "path on a file system", true);

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

    const bool human = parsed.has("human-readable");
    auto rows = collect_drives();
    if (!parsed.positionals.empty()) {
        std::vector<Row> filtered;
        for (const auto& raw : parsed.positionals) {
            std::string letter = drive_letter(raw);
            if (letter.empty()) {
                const auto path = utils::sys::path_from_utf8(raw);
                const auto abs = std::filesystem::absolute(path);
                letter = drive_letter(utils::sys::path_to_utf8(abs));
            }
            for (const auto& row : rows) {
                if (row.fs.size() >= 1 && letter.size() == 1 && row.fs[0] == letter[0]) {
                    filtered.push_back(row);
                }
            }
        }
        rows = std::move(filtered);
    }
    if (rows.empty()) {
        utils::output::writeln_err("df: no file systems matched");
        return 1;
    }

    auto fmt = [&](std::uint64_t bytes) {
        if (human) {
            return utils::theme::format_size(bytes, true, false);
        }
        return std::to_string((bytes + 1023) / 1024);
    };

    std::size_t sw = 4, uw = 4, aw = 5;
    for (const auto& row : rows) {
        sw = std::max(sw, fmt(row.size).size());
        uw = std::max(uw, fmt(row.used).size());
        aw = std::max(aw, fmt(row.avail).size());
    }
    const std::string size_h = human ? "Size" : "1K-blocks";
    sw = std::max(sw, size_h.size());

    std::ostringstream head;
    head << std::left << std::setw(12) << "Filesystem" << ' ' << std::right << std::setw(static_cast<int>(sw))
         << size_h << ' ' << std::setw(static_cast<int>(uw)) << "Used" << ' '
         << std::setw(static_cast<int>(aw)) << "Avail" << " Use% Mounted on";
    utils::output::writeln(head.str());
    for (const auto& row : rows) {
        std::ostringstream line;
        line << std::left << std::setw(12) << row.fs << ' ' << std::right
             << std::setw(static_cast<int>(sw)) << fmt(row.size) << ' '
             << std::setw(static_cast<int>(uw)) << fmt(row.used) << ' '
             << std::setw(static_cast<int>(aw)) << fmt(row.avail) << ' ' << std::setw(3)
             << row.pct << "% " << row.mounted;
        utils::output::writeln(line.str());
    }
    return 0;
}
