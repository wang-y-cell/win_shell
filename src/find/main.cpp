#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/sys.h"

#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <system_error>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace {

struct Cmp {
    char op = '=';
    std::int64_t value = 0;
};

bool parse_cmp(const std::string& spec, Cmp& out, std::string& error) {
    if (spec.empty()) {
        error = "find: invalid argument to predicate";
        return false;
    }
    std::size_t i = 0;
    if (spec[0] == '+' || spec[0] == '-') {
        out.op = spec[0];
        ++i;
    } else {
        out.op = '=';
    }
    char* end = nullptr;
    out.value = std::strtoll(spec.c_str() + i, &end, 10);
    if (end == spec.c_str() + i) {
        error = "find: invalid argument '" + spec + "'";
        return false;
    }
    return true;
}

bool parse_size(const std::string& spec, Cmp& out, std::string& error) {
    if (spec.empty()) {
        error = "find: invalid argument to `-size`";
        return false;
    }
    std::size_t i = 0;
    if (spec[0] == '+' || spec[0] == '-') {
        out.op = spec[0];
        ++i;
    } else {
        out.op = '=';
    }
    char* end = nullptr;
    const auto n = std::strtoll(spec.c_str() + i, &end, 10);
    if (end == spec.c_str() + i) {
        error = "find: invalid argument '" + spec + "' to `-size`";
        return false;
    }
    std::int64_t mul = 512;
    if (end && *end) {
        switch (*end) {
            case 'b':
            case 'c':
                mul = 1;
                break;
            case 'k':
            case 'K':
                mul = 1024;
                break;
            case 'm':
            case 'M':
                mul = 1024 * 1024;
                break;
            case 'g':
            case 'G':
                mul = 1024LL * 1024 * 1024;
                break;
            default:
                error = "find: invalid argument '" + spec + "' to `-size`";
                return false;
        }
        if (*(end + 1) != '\0') {
            error = "find: invalid argument '" + spec + "' to `-size`";
            return false;
        }
    }
    out.value = n * mul;
    return true;
}

bool cmp_ok(const Cmp& c, std::int64_t actual) {
    if (c.op == '+') {
        return actual > c.value;
    }
    if (c.op == '-') {
        return actual < c.value;
    }
    return actual == c.value;
}

bool match_glob(const std::string& name, const std::string& pattern, bool icase) {
    auto fold = [&](char ch) {
        return icase ? static_cast<char>(std::tolower(static_cast<unsigned char>(ch))) : ch;
    };
    std::size_t ni = 0;
    std::size_t pi = 0;
    std::size_t star = std::string::npos;
    std::size_t star_n = 0;
    while (ni < name.size()) {
        if (pi < pattern.size() && (pattern[pi] == '?' || fold(pattern[pi]) == fold(name[ni]))) {
            ++ni;
            ++pi;
        } else if (pi < pattern.size() && pattern[pi] == '*') {
            star = pi++;
            star_n = ni;
        } else if (star != std::string::npos) {
            pi = star + 1;
            ni = ++star_n;
        } else {
            return false;
        }
    }
    while (pi < pattern.size() && pattern[pi] == '*') {
        ++pi;
    }
    return pi == pattern.size();
}

#ifdef _WIN32
std::int64_t filetime_age_seconds(const FILETIME& ft) {
    FILETIME now{};
    GetSystemTimeAsFileTime(&now);
    ULARGE_INTEGER a{}, b{};
    a.LowPart = now.dwLowDateTime;
    a.HighPart = now.dwHighDateTime;
    b.LowPart = ft.dwLowDateTime;
    b.HighPart = ft.dwHighDateTime;
    if (a.QuadPart < b.QuadPart) {
        return 0;
    }
    return static_cast<std::int64_t>((a.QuadPart - b.QuadPart) / 10000000ULL);
}
#endif

struct Filters {
    std::string name;
    std::string iname;
    std::string path;
    std::string newer;
    std::vector<std::string> exec;
    char type = 0;
    bool has_mtime = false;
    bool has_mmin = false;
    bool has_atime = false;
    bool has_ctime = false;
    bool has_size = false;
    bool has_maxdepth = false;
    Cmp mtime;
    Cmp mmin;
    Cmp atime;
    Cmp ctime;
    Cmp size;
    int maxdepth = 0;
};

bool match_entry(const fs::path& path, const Filters& f) {
    const std::string name = utils::fsutil::filename_utf8(path);
    if (!f.name.empty() && !match_glob(name, f.name, false)) {
        return false;
    }
    if (!f.iname.empty() && !match_glob(name, f.iname, true)) {
        return false;
    }
    if (!f.path.empty()) {
        const std::string full = utils::sys::path_to_utf8(path);
        if (!match_glob(full, f.path, false) && !match_glob(name, f.path, false)) {
            return false;
        }
    }

    std::error_code ec;
    const bool is_dir = fs::is_directory(path, ec);
    const bool is_file = fs::is_regular_file(path, ec);
    bool is_link = fs::is_symlink(path, ec);
#ifdef _WIN32
    const DWORD attr = GetFileAttributesW(path.c_str());
    if (attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_REPARSE_POINT)) {
        is_link = true;
    }
#endif
    if (f.type == 'f' && !is_file) {
        return false;
    }
    if (f.type == 'd' && !is_dir) {
        return false;
    }
    if (f.type == 'l' && !is_link) {
        return false;
    }

#ifdef _WIN32
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &data)) {
        if (f.has_mtime) {
            const auto days = filetime_age_seconds(data.ftLastWriteTime) / 86400;
            if (!cmp_ok(f.mtime, days)) {
                return false;
            }
        }
        if (f.has_mmin) {
            const auto mins = filetime_age_seconds(data.ftLastWriteTime) / 60;
            if (!cmp_ok(f.mmin, mins)) {
                return false;
            }
        }
        if (f.has_atime) {
            const auto days = filetime_age_seconds(data.ftLastAccessTime) / 86400;
            if (!cmp_ok(f.atime, days)) {
                return false;
            }
        }
        if (f.has_ctime) {
            const auto days = filetime_age_seconds(data.ftCreationTime) / 86400;
            if (!cmp_ok(f.ctime, days)) {
                return false;
            }
        }
        if (!f.newer.empty()) {
            WIN32_FILE_ATTRIBUTE_DATA ref{};
            const auto ref_path = utils::sys::path_from_utf8(f.newer);
            if (!GetFileAttributesExW(ref_path.c_str(), GetFileExInfoStandard, &ref)) {
                return false;
            }
            ULARGE_INTEGER self{}, other{};
            self.LowPart = data.ftLastWriteTime.dwLowDateTime;
            self.HighPart = data.ftLastWriteTime.dwHighDateTime;
            other.LowPart = ref.ftLastWriteTime.dwLowDateTime;
            other.HighPart = ref.ftLastWriteTime.dwHighDateTime;
            if (self.QuadPart <= other.QuadPart) {
                return false;
            }
        }
        if (f.has_size) {
            if (is_dir) {
                return false;
            }
            ULARGE_INTEGER sz{};
            sz.LowPart = data.nFileSizeLow;
            sz.HighPart = data.nFileSizeHigh;
            if (!cmp_ok(f.size, static_cast<std::int64_t>(sz.QuadPart))) {
                return false;
            }
        }
    }
#else
    (void)f;
#endif
    return true;
}

#ifdef _WIN32
std::wstring quote_win(const std::wstring& text) {
    std::wstring out = L"\"";
    for (wchar_t c : text) {
        if (c == L'"') {
            out += L'\\';
        }
        out += c;
    }
    out += L'"';
    return out;
}

bool run_exec(const std::vector<std::string>& tmpl, const std::string& path) {
    std::wstring cmd;
    for (std::size_t i = 0; i < tmpl.size(); ++i) {
        std::string arg = tmpl[i];
        const auto pos = arg.find("{}");
        if (pos != std::string::npos) {
            arg.replace(pos, 2, path);
        }
        if (i) {
            cmd += L' ';
        }
        cmd += quote_win(utils::sys::utf8_to_wide(arg));
    }
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(L'\0');
    if (!CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si,
                        &pi)) {
        return false;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return true;
}
#endif

void walk(const fs::path& root, const std::string& display, const Filters& f, int depth) {
    if (f.has_maxdepth && depth > f.maxdepth) {
        return;
    }
    if (match_entry(root, f)) {
        if (!f.exec.empty()) {
#ifdef _WIN32
            run_exec(f.exec, display);
#endif
        } else {
            utils::output::writeln(display);
        }
    }
    if (f.has_maxdepth && depth >= f.maxdepth) {
        return;
    }
    std::error_code ec;
    if (!fs::is_directory(root, ec)) {
        return;
    }
    for (auto it = fs::directory_iterator(root, fs::directory_options::skip_permission_denied, ec);
         it != fs::directory_iterator(); it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        const std::string child =
            display + "\\" + utils::fsutil::filename_utf8(it->path());
        walk(it->path(), child, f, depth + 1);
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    const auto args = utils::sys::utf8_argv(argc, argv);
    Filters f;
    std::vector<std::string> roots;
    std::string error;

    for (std::size_t i = 1; i < args.size(); ++i) {
        const auto& tok = args[i];
        auto need = [&](const char* name) -> const std::string* {
            if (i + 1 >= args.size()) {
                error = std::string("find: missing argument to `") + name + "`";
                return nullptr;
            }
            return &args[++i];
        };
        if (tok == "-name") {
            const auto* v = need("-name");
            if (!v) {
                utils::output::writeln_err(error);
                return 1;
            }
            f.name = *v;
        } else if (tok == "-iname") {
            const auto* v = need("-iname");
            if (!v) {
                utils::output::writeln_err(error);
                return 1;
            }
            f.iname = *v;
        } else if (tok == "-type") {
            const auto* v = need("-type");
            if (!v) {
                utils::output::writeln_err(error);
                return 1;
            }
            if (v->size() != 1 || (*v != "f" && *v != "d" && *v != "l" && *v != "F" && *v != "D" &&
                                   *v != "L")) {
                utils::output::writeln_err("find: invalid argument `" + *v + "' to `-type'");
                return 1;
            }
            f.type = static_cast<char>(std::tolower(static_cast<unsigned char>((*v)[0])));
        } else if (tok == "-mtime") {
            const auto* v = need("-mtime");
            if (!v || !parse_cmp(*v, f.mtime, error)) {
                utils::output::writeln_err(error);
                return 1;
            }
            f.has_mtime = true;
        } else if (tok == "-mmin") {
            const auto* v = need("-mmin");
            if (!v || !parse_cmp(*v, f.mmin, error)) {
                utils::output::writeln_err(error);
                return 1;
            }
            f.has_mmin = true;
        } else if (tok == "-atime") {
            const auto* v = need("-atime");
            if (!v || !parse_cmp(*v, f.atime, error)) {
                utils::output::writeln_err(error);
                return 1;
            }
            f.has_atime = true;
        } else if (tok == "-ctime") {
            const auto* v = need("-ctime");
            if (!v || !parse_cmp(*v, f.ctime, error)) {
                utils::output::writeln_err(error);
                return 1;
            }
            f.has_ctime = true;
        } else if (tok == "-size") {
            const auto* v = need("-size");
            if (!v || !parse_size(*v, f.size, error)) {
                utils::output::writeln_err(error);
                return 1;
            }
            f.has_size = true;
        } else if (tok == "-path") {
            const auto* v = need("-path");
            if (!v) {
                utils::output::writeln_err(error);
                return 1;
            }
            f.path = *v;
        } else if (tok == "-newer") {
            const auto* v = need("-newer");
            if (!v) {
                utils::output::writeln_err(error);
                return 1;
            }
            f.newer = *v;
        } else if (tok == "-exec") {
            f.exec.clear();
            bool closed = false;
            while (i + 1 < args.size()) {
                const auto& next = args[++i];
                if (next == ";") {
                    closed = true;
                    break;
                }
                f.exec.push_back(next);
            }
            if (!closed || f.exec.empty()) {
                utils::output::writeln_err("find: missing argument to `-exec'");
                return 1;
            }
        } else if (tok == "-maxdepth") {
            const auto* v = need("-maxdepth");
            if (!v) {
                utils::output::writeln_err(error);
                return 1;
            }
            char* end = nullptr;
            const long n = std::strtol(v->c_str(), &end, 10);
            if (end == v->c_str() || *end != '\0' || n < 0) {
                utils::output::writeln_err("find: Invalid argument `" + *v + "' to `-maxdepth'");
                return 1;
            }
            f.maxdepth = static_cast<int>(n);
            f.has_maxdepth = true;
        } else if (tok == "--help") {
            utils::output::writeln("Usage: find [PATH...] [expression]");
            return 0;
        } else if (!tok.empty() && tok[0] == '-') {
            utils::output::writeln_err("find: unknown predicate `" + tok + "'");
            return 1;
        } else {
            roots.push_back(tok);
        }
    }

    if (!utils::sys::stdin_is_tty()) {
        for (const auto& line : utils::sys::read_stdin_lines()) {
            if (!line.empty()) {
                roots.push_back(line);
            }
        }
    }
    roots = utils::fsutil::expand_globs(roots);
    if (roots.empty()) {
        roots.emplace_back(".");
    }

    for (const auto& raw : roots) {
        const auto path = utils::sys::path_from_utf8(raw);
        if (!utils::fsutil::exists(path)) {
            utils::output::writeln_err("find: `" + raw + "': No such file or directory");
            continue;
        }
        walk(path, raw, f, 0);
    }
    return 0;
}
