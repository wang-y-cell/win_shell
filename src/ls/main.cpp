#include "utils/color.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/width.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <string>
#include <system_error>
#include <utility>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <aclapi.h>
#include <shellapi.h>
#endif

namespace fs = std::filesystem;

namespace {

using utils::color::ColorSpec;
using utils::color::rgb;

const ColorSpec kGreen = rgb(152, 195, 121);
const ColorSpec kBlue = rgb(97, 175, 239);
const ColorSpec kRed = rgb(224, 108, 117);
const ColorSpec kPurple = rgb(255, 85, 255);
const ColorSpec kWhite = rgb(220, 223, 228);
const ColorSpec kGray = rgb(192, 192, 192);
const ColorSpec kDarkGray = rgb(128, 128, 128);

enum class SortMode { Name, Time, Size, Extension, None };
enum class TimeKind { Mtime, Atime, Ctime };
enum class Layout { Long, Comma, One, Columns, Across };

struct LsOptions {
    bool all = false;
    bool almost_all = false;
    bool long_fmt = false;
    bool human = false;
    bool si = false;
    bool one_per_line = false;
    bool directory_only = false;
    bool reverse = false;
    bool recursive = false;
    bool classify = false;
    bool slash_dirs = false;
    bool quote = false;
    bool hide_backup = false;
    bool show_blocks = false;
    bool numeric_ids = false;
    bool show_inode = false;
    bool follow_all = false;
    bool follow_cmd = false;
    bool dirs_first = false;
    bool comma = false;
    bool force_columns = false;
    bool row_major = false;
    bool kilo_blocks = false;
    bool full_time = false;
    SortMode sort = SortMode::Name;
    TimeKind time_kind = TimeKind::Mtime;
    int width = 0;
    std::string hide;
    std::string ignore;
    std::vector<std::string> paths;
};

struct Entry {
    fs::path path;
    std::string name;
    bool is_dir = false;
    bool is_link = false;
    std::uint64_t size = 0;
    std::uint64_t inode = 0;
#ifdef _WIN32
    DWORD attrs = 0;
    FILETIME mtime{};
    FILETIME atime{};
    FILETIME ctime{};
#endif
};

std::string wide_to_utf8(const wchar_t* text, int length = -1) {
#ifdef _WIN32
    if (text == nullptr || length == 0 || (length < 0 && text[0] == L'\0')) {
        return {};
    }
    const int n = WideCharToMultiByte(CP_UTF8, 0, text, length, nullptr, 0, nullptr, nullptr);
    if (n <= 0) {
        return {};
    }
    std::string out(static_cast<std::size_t>(n), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, length, out.data(), n, nullptr, nullptr);
    if (!out.empty() && out.back() == '\0') {
        out.pop_back();
    }
    return out;
#else
    (void)text;
    (void)length;
    return {};
#endif
}

std::string path_to_utf8(const fs::path& path) {
#ifdef _WIN32
    const auto& w = path.native();
    return wide_to_utf8(w.c_str(), static_cast<int>(w.size()));
#else
    return path.u8string();
#endif
}

#ifdef _WIN32
std::wstring utf8_to_wide(const std::string& text) {
    if (text.empty()) {
        return {};
    }
    const int n = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()),
                                      nullptr, 0);
    if (n <= 0) {
        return {};
    }
    std::wstring out(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), static_cast<int>(text.size()), out.data(), n);
    return out;
}
#endif

std::vector<std::string> utf8_argv(int argc, char* argv[]) {
#ifdef _WIN32
    int count = 0;
    LPWSTR* wargv = CommandLineToArgvW(GetCommandLineW(), &count);
    if (wargv != nullptr && count > 0) {
        std::vector<std::string> args;
        args.reserve(static_cast<std::size_t>(count));
        for (int i = 0; i < count; ++i) {
            args.push_back(wide_to_utf8(wargv[i]));
        }
        LocalFree(wargv);
        return args;
    }
#endif
    std::vector<std::string> args;
    args.reserve(argc > 0 ? static_cast<std::size_t>(argc) : 0);
    for (int i = 0; i < argc; ++i) {
        args.emplace_back(argv[i] ? argv[i] : "");
    }
    return args;
}

std::string to_lower_ascii(std::string text) {
    for (char& c : text) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return text;
}

std::string extension_of(const std::string& name) {
    const auto pos = name.find_last_of('.');
    if (pos == std::string::npos || pos == 0 || pos + 1 == name.size()) {
        return {};
    }
    return to_lower_ascii(name.substr(pos));
}

bool glob_match(const std::string& name, const std::string& pattern) {
    const std::string n = to_lower_ascii(name);
    const std::string p = to_lower_ascii(pattern);
    const std::size_t nlen = n.size();
    const std::size_t plen = p.size();
    std::size_t ni = 0;
    std::size_t pi = 0;
    std::size_t star = std::string::npos;
    std::size_t match = 0;
    while (ni < nlen) {
        if (pi < plen && (p[pi] == '?' || p[pi] == n[ni])) {
            ++ni;
            ++pi;
        } else if (pi < plen && p[pi] == '*') {
            star = pi++;
            match = ni;
        } else if (star != std::string::npos) {
            pi = star + 1;
            ni = ++match;
        } else {
            return false;
        }
    }
    while (pi < plen && p[pi] == '*') {
        ++pi;
    }
    return pi == plen;
}

ColorSpec item_color(const Entry& entry) {
    if (entry.is_dir) {
        return kBlue;
    }
    if (entry.is_link) {
        return kPurple;
    }
    const std::string ext = extension_of(entry.name);
    if (ext == ".exe" || ext == ".bat" || ext == ".cmd" || ext == ".ps1") {
        return kGreen;
    }
    if (ext == ".zip" || ext == ".7z" || ext == ".rar" || ext == ".tar" || ext == ".gz") {
        return kRed;
    }
    if (ext == ".jpg" || ext == ".png" || ext == ".gif") {
        return kPurple;
    }
    return kWhite;
}

bool is_hidden(const Entry& entry) {
    if (!entry.name.empty() && entry.name[0] == '.') {
        return true;
    }
#ifdef _WIN32
    return (entry.attrs & FILE_ATTRIBUTE_HIDDEN) != 0;
#else
    return false;
#endif
}

bool is_backup(const Entry& entry) {
    return !entry.name.empty() && entry.name.back() == '~';
}

bool should_skip(const Entry& entry, const LsOptions& opt) {
    if (entry.name == "." || entry.name == "..") {
        return false;
    }
    if (!opt.ignore.empty() && glob_match(entry.name, opt.ignore)) {
        return true;
    }
    if (!opt.all && !opt.almost_all) {
        if (is_hidden(entry)) {
            return true;
        }
        if (!opt.hide.empty() && glob_match(entry.name, opt.hide)) {
            return true;
        }
    }
    if (opt.hide_backup && is_backup(entry)) {
        return true;
    }
    return false;
}

#ifdef _WIN32
FILETIME time_of(const Entry& entry, TimeKind kind) {
    if (kind == TimeKind::Atime) {
        return entry.atime;
    }
    if (kind == TimeKind::Ctime) {
        return entry.ctime;
    }
    return entry.mtime;
}

std::uint64_t filetime_u64(const FILETIME& ft) {
    ULARGE_INTEGER value{};
    value.LowPart = ft.dwLowDateTime;
    value.HighPart = ft.dwHighDateTime;
    return value.QuadPart;
}
#endif

Entry make_entry(const fs::path& path, const std::string& display_name, bool follow) {
    Entry entry;
    entry.path = path;
    entry.name = display_name.empty() ? path_to_utf8(path.filename()) : display_name;
    if (entry.name.empty()) {
        entry.name = path_to_utf8(path);
    }

#ifdef _WIN32
    DWORD flags = FILE_FLAG_BACKUP_SEMANTICS;
    if (!follow) {
        flags |= FILE_FLAG_OPEN_REPARSE_POINT;
    }
    HANDLE handle = CreateFileW(path.c_str(), 0,
                                FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr,
                                OPEN_EXISTING, flags, nullptr);
    if (handle != INVALID_HANDLE_VALUE) {
        BY_HANDLE_FILE_INFORMATION info{};
        if (GetFileInformationByHandle(handle, &info)) {
            entry.attrs = info.dwFileAttributes;
            entry.is_dir = (info.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
            entry.is_link = !follow && (info.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
            ULARGE_INTEGER size{};
            size.LowPart = info.nFileSizeLow;
            size.HighPart = info.nFileSizeHigh;
            entry.size = size.QuadPart;
            entry.mtime = info.ftLastWriteTime;
            entry.atime = info.ftLastAccessTime;
            entry.ctime = info.ftCreationTime;
            entry.inode = (static_cast<std::uint64_t>(info.nFileIndexHigh) << 32) |
                          static_cast<std::uint64_t>(info.nFileIndexLow);
            CloseHandle(handle);
            return entry;
        }
        CloseHandle(handle);
    }

    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) {
        entry.attrs = data.dwFileAttributes;
        entry.is_dir = (data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        entry.is_link = (data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) != 0;
        ULARGE_INTEGER size{};
        size.LowPart = data.nFileSizeLow;
        size.HighPart = data.nFileSizeHigh;
        entry.size = size.QuadPart;
        entry.mtime = data.ftLastWriteTime;
        entry.atime = data.ftLastAccessTime;
        entry.ctime = data.ftCreationTime;
        return entry;
    }
#endif

    std::error_code ec;
    const auto status = follow ? fs::status(path, ec) : fs::symlink_status(path, ec);
    entry.is_dir = status.type() == fs::file_type::directory;
    entry.is_link = status.type() == fs::file_type::symlink;
    if (!entry.is_dir) {
        const auto size = fs::file_size(path, ec);
        if (!ec) {
            entry.size = size;
        }
    }
    return entry;
}

bool name_less(const Entry& a, const Entry& b) {
#ifdef _WIN32
    const std::wstring wa = utf8_to_wide(a.name);
    const std::wstring wb = utf8_to_wide(b.name);
    const int cmp = CompareStringEx(LOCALE_NAME_USER_DEFAULT, LINGUISTIC_IGNORECASE, wa.c_str(),
                                    static_cast<int>(wa.size()), wb.c_str(),
                                    static_cast<int>(wb.size()), nullptr, nullptr, 0);
    if (cmp == CSTR_LESS_THAN) {
        return true;
    }
    if (cmp == CSTR_GREATER_THAN) {
        return false;
    }
    return a.name < b.name;
#else
    return a.name < b.name;
#endif
}

bool entry_less(const Entry& a, const Entry& b, const LsOptions& opt) {
    if (opt.dirs_first && a.is_dir != b.is_dir) {
        return a.is_dir;
    }

    bool less = false;
    bool equal = false;
    switch (opt.sort) {
        case SortMode::None:
            return false;
        case SortMode::Size:
            if (a.size != b.size) {
                less = a.size > b.size;
            } else {
                equal = true;
            }
            break;
        case SortMode::Time:
#ifdef _WIN32
        {
            const auto ta = filetime_u64(time_of(a, opt.time_kind));
            const auto tb = filetime_u64(time_of(b, opt.time_kind));
            if (ta != tb) {
                less = ta > tb;
            } else {
                equal = true;
            }
            break;
        }
#else
            equal = true;
            break;
#endif
        case SortMode::Extension: {
            const std::string ea = extension_of(a.name);
            const std::string eb = extension_of(b.name);
            if (ea != eb) {
                less = ea < eb;
            } else {
                equal = true;
            }
            break;
        }
        case SortMode::Name:
        default:
            less = name_less(a, b);
            break;
    }
    if (equal) {
        less = name_less(a, b);
    }
    return less;
}

void sort_entries(std::vector<Entry>& entries, const LsOptions& opt) {
    if (opt.sort == SortMode::None) {
        if (opt.dirs_first) {
            std::stable_partition(entries.begin(), entries.end(),
                                  [](const Entry& entry) { return entry.is_dir; });
        }
        if (opt.reverse) {
            std::reverse(entries.begin(), entries.end());
        }
        return;
    }
    std::sort(entries.begin(), entries.end(),
              [&](const Entry& a, const Entry& b) { return entry_less(a, b, opt); });
    if (!opt.reverse) {
        return;
    }
    if (opt.dirs_first) {
        const auto mid = std::stable_partition(entries.begin(), entries.end(),
                                               [](const Entry& entry) { return entry.is_dir; });
        std::reverse(entries.begin(), mid);
        std::reverse(mid, entries.end());
        return;
    }
    std::reverse(entries.begin(), entries.end());
}

std::string format_mode(const Entry& entry) {
#ifdef _WIN32
    std::string mode = "------";
    if (entry.is_dir) {
        mode[0] = 'd';
    } else if (entry.is_link) {
        mode[0] = 'l';
    }
    if (entry.attrs & FILE_ATTRIBUTE_ARCHIVE) {
        mode[1] = 'a';
    }
    if (entry.attrs & FILE_ATTRIBUTE_READONLY) {
        mode[2] = 'r';
    }
    if (entry.attrs & FILE_ATTRIBUTE_HIDDEN) {
        mode[3] = 'h';
    }
    if (entry.attrs & FILE_ATTRIBUTE_SYSTEM) {
        mode[4] = 's';
    }
    if (entry.attrs & FILE_ATTRIBUTE_REPARSE_POINT) {
        mode[5] = 'l';
    }
    return mode;
#else
    if (entry.is_dir) {
        return "d-----";
    }
    if (entry.is_link) {
        return "l-----";
    }
    return "------";
#endif
}

std::string format_time(const Entry& entry, const LsOptions& opt) {
#ifdef _WIN32
    const FILETIME ft = time_of(entry, opt.time_kind);
    SYSTEMTIME utc{};
    SYSTEMTIME local{};
    if (!FileTimeToSystemTime(&ft, &utc)) {
        return opt.full_time ? "0000-00-00 00:00:00.000" : "0000-00-00 00:00";
    }
    if (!SystemTimeToTzSpecificLocalTime(nullptr, &utc, &local)) {
        local = utc;
    }
    char buf[40];
    if (opt.full_time) {
        std::snprintf(buf, sizeof(buf), " %04u-%02u-%02u %02u:%02u:%02u.%03u", local.wYear,
                      local.wMonth, local.wDay, local.wHour, local.wMinute, local.wSecond,
                      local.wMilliseconds);
        return buf + 1;
    }
    std::snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u", local.wYear, local.wMonth,
                  local.wDay, local.wHour, local.wMinute);
    return buf;
#else
    (void)entry;
    return opt.full_time ? "0000-00-00 00:00:00.000" : "0000-00-00 00:00";
#endif
}

std::string format_size(const Entry& entry, const LsOptions& opt) {
    if (entry.is_dir) {
        return "-";
    }
    if (!opt.human) {
        return std::to_string(entry.size);
    }

    const double base = opt.si ? 1000.0 : 1024.0;
    static const char* units[] = {"B", "K", "M", "G", "T", "P"};
    double size = static_cast<double>(entry.size);
    int unit = 0;
    while (size >= base && unit < 5) {
        size /= base;
        ++unit;
    }

    char buf[32];
    if (unit == 0) {
        std::snprintf(buf, sizeof(buf), "%dB", static_cast<int>(size));
    } else if (size >= 10.0) {
        std::snprintf(buf, sizeof(buf), "%.0f%s", size, units[unit]);
    } else {
        std::snprintf(buf, sizeof(buf), "%.1f%s", size, units[unit]);
    }
    return buf;
}

std::uint64_t block_size_of(const LsOptions& opt) {
    if (opt.si && !opt.kilo_blocks) {
        return 1000;
    }
    return 1024;
}

std::uint64_t block_count(const Entry& entry, const LsOptions& opt) {
    if (entry.is_dir) {
        return 0;
    }
    const std::uint64_t blk = block_size_of(opt);
    if (entry.size == 0) {
        return 0;
    }
    return (entry.size + blk - 1) / blk;
}

std::string file_owner(const Entry& entry, bool numeric) {
    if (numeric) {
        return "0";
    }
#ifdef _WIN32
    PSID owner_sid = nullptr;
    PSECURITY_DESCRIPTOR sd = nullptr;
    const DWORD status =
        GetNamedSecurityInfoW(entry.path.c_str(), SE_FILE_OBJECT, OWNER_SECURITY_INFORMATION,
                              &owner_sid, nullptr, nullptr, nullptr, &sd);
    if (status != ERROR_SUCCESS || owner_sid == nullptr) {
        return "-";
    }
    wchar_t name[256];
    wchar_t domain[256];
    DWORD name_len = 256;
    DWORD domain_len = 256;
    SID_NAME_USE use{};
    std::string out = "-";
    if (LookupAccountSidW(nullptr, owner_sid, name, &name_len, domain, &domain_len, &use)) {
        out = wide_to_utf8(name);
    }
    LocalFree(sd);
    return out.empty() ? "-" : out;
#else
    (void)entry;
    return "-";
#endif
}

std::string file_group(bool numeric) {
    return numeric ? "0" : "-";
}

std::string quote_name(const std::string& name) {
    std::string out = "\"";
    for (char c : name) {
        if (c == '"' || c == '\\') {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    out.push_back('"');
    return out;
}

bool is_executable_name(const std::string& name) {
    const std::string ext = extension_of(name);
    return ext == ".exe" || ext == ".bat" || ext == ".cmd" || ext == ".ps1" || ext == ".com";
}

std::string classify_suffix(const Entry& entry, const LsOptions& opt) {
    if (entry.is_dir && (opt.classify || opt.slash_dirs)) {
        return "/";
    }
    if (opt.classify) {
        if (entry.is_link) {
            return "@";
        }
        if (is_executable_name(entry.name)) {
            return "*";
        }
    }
    return {};
}

std::string display_name(const Entry& entry, const LsOptions& opt) {
    std::string name = opt.quote ? quote_name(entry.name) : entry.name;
    name += classify_suffix(entry, opt);
    return name;
}

std::string pad_left(const std::string& text, std::size_t width) {
    if (text.size() >= width) {
        return text;
    }
    return std::string(width - text.size(), ' ') + text;
}

std::string pad_right(const std::string& text, std::size_t width) {
    if (text.size() >= width) {
        return text;
    }
    return text + std::string(width - text.size(), ' ');
}

std::string meta_prefix(const Entry& entry, const LsOptions& opt, std::size_t inode_w,
                        std::size_t block_w) {
    std::string prefix;
    if (opt.show_inode) {
        prefix += pad_left(std::to_string(entry.inode), inode_w);
        prefix += ' ';
    }
    if (opt.show_blocks) {
        prefix += pad_left(std::to_string(block_count(entry, opt)), block_w);
        prefix += ' ';
    }
    return prefix;
}

void measure_meta(const std::vector<Entry>& entries, const LsOptions& opt, std::size_t& inode_w,
                  std::size_t& block_w) {
    inode_w = 1;
    block_w = 1;
    for (const auto& entry : entries) {
        inode_w = std::max(inode_w, std::to_string(entry.inode).size());
        block_w = std::max(block_w, std::to_string(block_count(entry, opt)).size());
    }
}

int console_width(const LsOptions& opt) {
    if (opt.width > 0) {
        return opt.width;
    }
    if (opt.width < 0) {
        return 65535;
    }
    return utils::output::terminal_width();
}

Layout layout_of(const LsOptions& opt) {
    if (opt.long_fmt) {
        return Layout::Long;
    }
    if (opt.comma) {
        return Layout::Comma;
    }
    if (opt.force_columns) {
        return opt.row_major ? Layout::Across : Layout::Columns;
    }
    if (opt.one_per_line || !utils::output::is_stdout_tty()) {
        return Layout::One;
    }
    return opt.row_major ? Layout::Across : Layout::Columns;
}

void write_long(const std::vector<Entry>& entries, const LsOptions& opt) {
    std::size_t inode_w = 1;
    std::size_t block_w = 1;
    measure_meta(entries, opt, inode_w, block_w);

    std::vector<std::string> sizes;
    std::vector<std::string> owners;
    std::vector<std::string> groups;
    sizes.reserve(entries.size());
    owners.reserve(entries.size());
    groups.reserve(entries.size());
    std::size_t size_width = 1;
    std::size_t owner_width = 1;
    std::size_t group_width = 1;
    for (const auto& entry : entries) {
        sizes.push_back(format_size(entry, opt));
        owners.push_back(file_owner(entry, opt.numeric_ids));
        groups.push_back(file_group(opt.numeric_ids));
        size_width = std::max(size_width, sizes.back().size());
        owner_width = std::max(owner_width, owners.back().size());
        group_width = std::max(group_width, groups.back().size());
    }

    for (std::size_t i = 0; i < entries.size(); ++i) {
        if (opt.show_inode || opt.show_blocks) {
            utils::output::write(meta_prefix(entries[i], opt, inode_w, block_w), kDarkGray);
        }
        utils::output::write(format_mode(entries[i]) + "  ", kBlue);
        utils::output::write(pad_right(owners[i], owner_width) + "  ", kGray);
        utils::output::write(pad_right(groups[i], group_width) + "  ", kGray);
        utils::output::write(format_time(entries[i], opt) + "  ", kGray);
        utils::output::write(pad_left(sizes[i], size_width) + "  ", kDarkGray);
        utils::output::writeln(display_name(entries[i], opt), item_color(entries[i]));
    }
}

void write_one(const std::vector<Entry>& entries, const LsOptions& opt) {
    std::size_t inode_w = 1;
    std::size_t block_w = 1;
    measure_meta(entries, opt, inode_w, block_w);
    for (const auto& entry : entries) {
        if (opt.show_inode || opt.show_blocks) {
            utils::output::write(meta_prefix(entry, opt, inode_w, block_w), kDarkGray);
        }
        utils::output::writeln(display_name(entry, opt), item_color(entry));
    }
}

void write_comma(const std::vector<Entry>& entries, const LsOptions& opt) {
    const int term_width = std::max(1, console_width(opt));
    std::string line;
    std::size_t line_w = 0;
    for (std::size_t i = 0; i < entries.size(); ++i) {
        std::string piece = display_name(entries[i], opt);
        if (i + 1 < entries.size()) {
            piece += ",";
        }
        const std::size_t piece_w = utils::width::display_width(piece);
        const std::size_t extra = line.empty() ? 0 : 1;
        if (!line.empty() && line_w + extra + piece_w > static_cast<std::size_t>(term_width)) {
            utils::output::writeln(line);
            line = piece;
            line_w = piece_w;
        } else {
            if (!line.empty()) {
                line += " ";
                line_w += 1;
            }
            line += piece;
            line_w += piece_w;
        }
    }
    if (!line.empty()) {
        utils::output::writeln(line);
    }
}

void write_grid(const std::vector<Entry>& entries, const LsOptions& opt, bool across) {
    const std::size_t count = entries.size();
    if (count == 0) {
        return;
    }

    std::size_t inode_w = 1;
    std::size_t block_w = 1;
    measure_meta(entries, opt, inode_w, block_w);

    std::vector<std::string> names(count);
    std::vector<std::string> prefixes(count);
    std::vector<std::size_t> visual(count);
    for (std::size_t i = 0; i < count; ++i) {
        prefixes[i] = meta_prefix(entries[i], opt, inode_w, block_w);
        names[i] = display_name(entries[i], opt);
        visual[i] = utils::width::display_width(prefixes[i] + names[i]);
    }

    const int term_width = std::max(1, console_width(opt));
    std::size_t best_cols = 1;
    std::size_t best_rows = count;
    std::vector<std::size_t> best_col_max(1, 0);
    for (std::size_t w : visual) {
        best_col_max[0] = std::max(best_col_max[0], w);
    }

    const std::size_t max_try = std::min(count, static_cast<std::size_t>(term_width));
    for (std::size_t cols = max_try; cols >= 1; --cols) {
        const std::size_t rows = (count + cols - 1) / cols;
        std::vector<std::size_t> col_max(cols, 0);
        for (std::size_t i = 0; i < count; ++i) {
            const std::size_t col = across ? (i % cols) : (i / rows);
            col_max[col] = std::max(col_max[col], visual[i]);
        }
        std::size_t total = 0;
        for (std::size_t c = 0; c < cols; ++c) {
            total += col_max[c];
            if (c + 1 < cols) {
                total += 2;
            }
        }
        if (total <= static_cast<std::size_t>(term_width)) {
            best_cols = cols;
            best_rows = rows;
            best_col_max = std::move(col_max);
            break;
        }
    }

    for (std::size_t row = 0; row < best_rows; ++row) {
        for (std::size_t col = 0; col < best_cols; ++col) {
            const std::size_t index = across ? (row * best_cols + col) : (col * best_rows + row);
            if (index >= count) {
                break;
            }
            if (!prefixes[index].empty()) {
                utils::output::write(prefixes[index], kDarkGray);
            }
            utils::output::write(names[index], item_color(entries[index]));

            bool more = false;
            for (std::size_t next = col + 1; next < best_cols; ++next) {
                const std::size_t ni = across ? (row * best_cols + next) : (next * best_rows + row);
                if (ni < count) {
                    more = true;
                    break;
                }
            }
            if (more) {
                const std::size_t pad = (best_col_max[col] + 2) - visual[index];
                if (pad > 0) {
                    utils::output::write(std::string(pad, ' '));
                }
            }
        }
        utils::output::writeln("");
    }
}

void write_entries(const std::vector<Entry>& entries, const LsOptions& opt) {
    if (entries.empty()) {
        return;
    }
    switch (layout_of(opt)) {
        case Layout::Long:
            write_long(entries, opt);
            break;
        case Layout::Comma:
            write_comma(entries, opt);
            break;
        case Layout::One:
            write_one(entries, opt);
            break;
        case Layout::Across:
            write_grid(entries, opt, true);
            break;
        case Layout::Columns:
            write_grid(entries, opt, false);
            break;
    }
}

bool collect_dir(const fs::path& dir, const LsOptions& opt, std::vector<Entry>& out,
                 const std::string& display) {
    std::error_code ec;
    fs::directory_iterator it(dir, fs::directory_options::skip_permission_denied, ec);
    if (ec) {
        utils::output::writeln_err("ls: cannot open directory '" + display + "': " + ec.message());
        return false;
    }

    if (opt.all) {
        out.push_back(make_entry(dir, ".", opt.follow_all));
        std::error_code parent_ec;
        const fs::path parent = fs::absolute(dir, parent_ec).parent_path();
        out.push_back(make_entry(parent_ec ? (dir / "..") : parent, "..", opt.follow_all));
    }

    const fs::directory_iterator end;
    for (; it != end; it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        Entry entry = make_entry(it->path(), path_to_utf8(it->path().filename()), opt.follow_all);
        if (should_skip(entry, opt)) {
            continue;
        }
        out.push_back(std::move(entry));
    }

    sort_entries(out, opt);
    return true;
}

std::string join_display(const std::string& parent, const std::string& child) {
    if (parent.empty() || parent == ".") {
        return child;
    }
    const bool use_slash = parent.find('/') != std::string::npos && parent.find('\\') == std::string::npos;
    return parent + (use_slash ? '/' : '\\') + child;
}

bool is_dot_name(const std::string& name) {
    return name == "." || name == "..";
}

bool list_directory(const fs::path& dir, const std::string& display, const LsOptions& opt,
                    bool show_header, bool& printed, bool& had_error) {
    if (show_header) {
        if (printed) {
            utils::output::writeln("");
        }
        utils::output::writeln(display + ":");
    }

    std::vector<Entry> entries;
    if (!collect_dir(dir, opt, entries, display)) {
        had_error = true;
        printed = true;
        return false;
    }
    write_entries(entries, opt);
    printed = true;

    if (!opt.recursive || opt.directory_only) {
        return true;
    }

    for (const auto& entry : entries) {
        if (!entry.is_dir || is_dot_name(entry.name)) {
            continue;
        }
        list_directory(entry.path, join_display(display, entry.name), opt, true, printed, had_error);
    }
    return true;
}

int run(const LsOptions& opt) {
    const std::vector<std::string> paths = opt.paths.empty() ? std::vector<std::string>{"."} : opt.paths;
    const bool follow_cmd = opt.follow_all || opt.follow_cmd;

    std::vector<Entry> files;
    std::vector<std::pair<std::string, fs::path>> dirs;
    bool had_error = false;

    for (const auto& raw : paths) {
#ifdef _WIN32
        const fs::path path(utf8_to_wide(raw));
#else
        const fs::path path(raw);
#endif
        std::error_code ec;
        const auto status = follow_cmd ? fs::status(path, ec) : fs::symlink_status(path, ec);
        if (ec || status.type() == fs::file_type::not_found) {
            utils::output::writeln_err("ls: cannot access '" + raw + "': No such file or directory");
            had_error = true;
            continue;
        }
        const bool is_dir = follow_cmd ? fs::is_directory(path, ec)
                                      : (status.type() == fs::file_type::directory);
        if (opt.directory_only || !is_dir) {
            files.push_back(make_entry(path, raw, follow_cmd));
        } else {
            dirs.emplace_back(raw, path);
        }
    }

    sort_entries(files, opt);
    {
        std::vector<Entry> dir_entries;
        dir_entries.reserve(dirs.size());
        for (const auto& dir : dirs) {
            dir_entries.push_back(make_entry(dir.second, dir.first, follow_cmd));
        }
        sort_entries(dir_entries, opt);
        std::vector<std::pair<std::string, fs::path>> ordered;
        ordered.reserve(dir_entries.size());
        for (const auto& entry : dir_entries) {
            ordered.emplace_back(entry.name, entry.path);
        }
        dirs = std::move(ordered);
    }

    const bool show_headers = files.size() + dirs.size() > 1 || opt.recursive;
    bool printed = false;

    if (!files.empty()) {
        write_entries(files, opt);
        printed = true;
    }

    for (const auto& dir : dirs) {
        const bool header = show_headers && !(files.empty() && dirs.size() == 1 && !opt.recursive);
        list_directory(dir.second, dir.first, opt, header, printed, had_error);
    }

    return had_error ? 2 : 0;
}

SortMode parse_sort(const std::string& text, bool& ok) {
    const std::string value = to_lower_ascii(text);
    ok = true;
    if (value == "name") {
        return SortMode::Name;
    }
    if (value == "time") {
        return SortMode::Time;
    }
    if (value == "size") {
        return SortMode::Size;
    }
    if (value == "extension" || value == "ext") {
        return SortMode::Extension;
    }
    if (value == "none") {
        return SortMode::None;
    }
    ok = false;
    return SortMode::Name;
}

TimeKind parse_time_kind(const std::string& text, bool& ok) {
    const std::string value = to_lower_ascii(text);
    ok = true;
    if (value == "mtime" || value == "modification") {
        return TimeKind::Mtime;
    }
    if (value == "atime" || value == "access" || value == "use") {
        return TimeKind::Atime;
    }
    if (value == "ctime" || value == "status" || value == "birth") {
        return TimeKind::Ctime;
    }
    ok = false;
    return TimeKind::Mtime;
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();

#ifdef LS_AS_LL
    utils::Parser parser("ll", "List directory contents (ls -alh)");
#else
    utils::Parser parser("ls", "List directory contents");
#endif
    parser.flag("a", "all", "do not ignore hidden entries")
        .flag("A", "almost-all", "do not list implied . and ..")
        .flag("l", "long", "use a long listing format")
        .flag("h", "human-readable", "with -l, print sizes in human readable form")
        .flag("", "si", "human-readable sizes in powers of 1000")
        .flag("1", "one", "list one file per line")
        .flag("d", "directory", "list directories themselves, not their contents")
        .flag("t", "sort-time", "sort by time, newest first")
        .flag("r", "reverse", "reverse order while sorting")
        .flag("S", "sort-size", "sort by file size, largest first")
        .flag("U", "sort-none", "do not sort; list entries in directory order")
        .flag("X", "sort-extension", "sort alphabetically by entry extension")
        .flag("R", "recursive", "list subdirectories recursively")
        .flag("F", "classify", "append indicator (one of */@) to entries")
        .flag("p", "indicator-slash", "append / indicator to directories")
        .flag("Q", "quote-name", "enclose entry names in double quotes")
        .flag("B", "ignore-backups", "do not list implied entries ending with ~")
        .flag("s", "size", "print the allocated size of each file, in blocks")
        .flag("k", "kibibytes", "default to 1024-byte blocks for -s")
        .flag("n", "numeric-uid-gid", "list numeric user and group IDs")
        .flag("i", "inode", "print the index number of each file")
        .flag("L", "dereference", "follow all symbolic links")
        .flag("H", "dereference-command-line", "follow symbolic links on the command line")
        .flag("m", "comma", "fill width with a comma separated list of entries")
        .flag("C", "columns", "list entries by columns")
        .flag("x", "across", "list entries by lines instead of by columns")
        .flag("", "group-directories-first", "group directories before files")
        .flag("", "full-time", "show full date and time")
        .flag("", "help", "show this help")
        .option("", "color", "WHEN", "colorize output: auto, always, never")
        .option("", "hide", "PATTERN", "do not list implied entries matching PATTERN")
        .option("I", "ignore", "PATTERN", "do not list implied entries matching PATTERN")
        .option("", "sort", "WORD", "sort by WORD: name, time, size, extension, none")
        .option("", "time", "WORD", "select time: mtime, atime, ctime")
        .option("w", "width", "COLS", "assume screen width instead of current value")
        .positional("FILE", "file or directory to list", true);

    const auto args = utf8_argv(argc, argv);
    const utils::ParseResult parsed = parser.parse(args);
    if (!parsed.ok) {
        utils::output::writeln_err(parsed.error);
        return 2;
    }
    if (parsed.has("help")) {
        utils::output::write(parser.help());
        return 0;
    }

    if (parsed.has("color")) {
        utils::output::ColorMode mode = utils::output::ColorMode::Auto;
        if (!utils::output::parse_color_mode(parsed.get("color"), mode)) {
            utils::output::writeln_err("ls: invalid --color value '" + parsed.get("color") + "'");
            return 2;
        }
        utils::output::set_color_mode(mode);
    }

    LsOptions opt;
    opt.all = parsed.has("all");
    opt.almost_all = parsed.has("almost-all") && !opt.all;
    opt.long_fmt = parsed.has("long");
    opt.human = parsed.has("human-readable") || parsed.has("si");
    opt.si = parsed.has("si");
    opt.one_per_line = parsed.has("one");
    opt.directory_only = parsed.has("directory");
    opt.reverse = parsed.has("reverse");
    opt.recursive = parsed.has("recursive");
    opt.classify = parsed.has("classify");
    opt.slash_dirs = parsed.has("indicator-slash");
    opt.quote = parsed.has("quote-name");
    opt.hide_backup = parsed.has("ignore-backups");
    opt.show_blocks = parsed.has("size");
    opt.kilo_blocks = parsed.has("kibibytes");
    opt.numeric_ids = parsed.has("numeric-uid-gid");
    opt.show_inode = parsed.has("inode");
    opt.follow_all = parsed.has("dereference");
    opt.follow_cmd = parsed.has("dereference-command-line");
    opt.dirs_first = parsed.has("group-directories-first");
    opt.comma = parsed.has("comma");
    opt.force_columns = parsed.has("columns");
    opt.row_major = parsed.has("across");
    opt.full_time = parsed.has("full-time");
    opt.hide = parsed.get("hide");
    opt.ignore = parsed.get("ignore");

    if (parsed.has("sort-none")) {
        opt.sort = SortMode::None;
    } else if (parsed.has("sort")) {
        bool ok = false;
        opt.sort = parse_sort(parsed.get("sort"), ok);
        if (!ok) {
            utils::output::writeln_err("ls: invalid --sort value '" + parsed.get("sort") + "'");
            return 2;
        }
    } else if (parsed.has("sort-size")) {
        opt.sort = SortMode::Size;
    } else if (parsed.has("sort-extension")) {
        opt.sort = SortMode::Extension;
    } else if (parsed.has("sort-time")) {
        opt.sort = SortMode::Time;
    }

    if (parsed.has("time")) {
        bool ok = false;
        opt.time_kind = parse_time_kind(parsed.get("time"), ok);
        if (!ok) {
            utils::output::writeln_err("ls: invalid --time value '" + parsed.get("time") + "'");
            return 2;
        }
    }

    if (parsed.has("width")) {
        try {
            opt.width = std::stoi(parsed.get("width"));
        } catch (...) {
            utils::output::writeln_err("ls: invalid --width value '" + parsed.get("width") + "'");
            return 2;
        }
        if (opt.width == 0) {
            opt.width = -1;
        }
    }

#ifdef LS_AS_LL
    opt.all = true;
    opt.almost_all = false;
    opt.long_fmt = true;
    opt.human = true;
#endif
    opt.paths = parsed.positionals;
    return run(opt);
}
