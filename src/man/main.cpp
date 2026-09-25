#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <cctype>
#include <cstdlib>
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

std::vector<std::string> split_path(const std::string& path) {
    std::vector<std::string> parts;
    std::string cur;
    for (char c : path) {
        if (c == ';' || c == ':') {
#ifdef _WIN32
            if (c == ':') {
                cur.push_back(c);
                continue;
            }
#endif
            if (!cur.empty()) {
                parts.push_back(cur);
                cur.clear();
            }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) {
        parts.push_back(cur);
    }
    return parts;
}

#ifdef _WIN32
std::vector<std::string> pathexts() {
    const char* ext = std::getenv("PATHEXT");
    std::vector<std::string> out;
    if (ext == nullptr) {
        return {".EXE", ".BAT", ".CMD", ".COM"};
    }
    std::string cur;
    for (const char* p = ext; *p; ++p) {
        if (*p == ';') {
            if (!cur.empty()) {
                out.push_back(cur);
                cur.clear();
            }
        } else {
            cur.push_back(static_cast<char>(std::toupper(static_cast<unsigned char>(*p))));
        }
    }
    if (!cur.empty()) {
        out.push_back(cur);
    }
    return out;
}

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
#endif

bool file_exists(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
}

std::filesystem::path find_command(const std::string& name) {
    const char* path_env = std::getenv("PATH");
    const auto dirs = split_path(path_env ? path_env : "");
#ifdef _WIN32
    const auto exts = pathexts();
#endif
    for (const auto& dir : dirs) {
        const auto base = utils::sys::path_from_utf8(dir) / utils::sys::path_from_utf8(name);
        std::vector<std::filesystem::path> candidates{base};
#ifdef _WIN32
        if (name.find('.') == std::string::npos) {
            for (const auto& ext : exts) {
                candidates.push_back(utils::sys::path_from_utf8(dir) /
                                     utils::sys::path_from_utf8(name + ext));
            }
        }
#endif
        for (const auto& cand : candidates) {
            if (file_exists(cand)) {
                return std::filesystem::absolute(cand);
            }
        }
    }
    return {};
}

std::string stem_lower(const std::filesystem::path& path) {
    std::string name = utils::sys::path_to_utf8(path.stem());
    for (char& c : name) {
        if (c >= 'A' && c <= 'Z') {
            c = static_cast<char>(c - 'A' + 'a');
        }
    }
    return name;
}

#ifdef _WIN32
int run_help(const std::filesystem::path& exe, const std::vector<std::string>& extra) {
    std::wstring cmd = quote_win(exe.wstring());
    bool has_help = false;
    for (const auto& arg : extra) {
        if (arg == "--help") {
            has_help = true;
        }
        cmd += L' ';
        cmd += quote_win(utils::sys::utf8_to_wide(arg));
    }
    if (!has_help) {
        cmd += L" --help";
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(L'\0');
    if (!CreateProcessW(exe.c_str(), buf.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si,
                        &pi)) {
        utils::output::writeln_err("man: cannot run '" + utils::sys::path_to_utf8(exe) + "'");
        return 1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return static_cast<int>(code);
}
#endif

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("man", "Show a command's --help text");
    parser.flag("", "help", "show this help")
        .positional("COMMAND", "command to explain", false)
        .positional("ARG", "extra arguments forwarded to COMMAND", true);

    const auto args = utils::sys::utf8_argv(argc, argv);
    const auto parsed = parser.parse(args);
    if (!parsed.ok) {
        utils::output::writeln_err(parsed.error);
        return 1;
    }
    if (parsed.has("help") || parsed.positionals.empty()) {
        utils::output::write(parser.help());
        return 0;
    }

    const std::string name = parsed.positionals[0];
    if (name == "man") {
        utils::output::write(parser.help());
        return 0;
    }

    const auto exe = find_command(name);
    if (exe.empty()) {
        utils::output::writeln_err("man: no manual entry for " + name);
        return 1;
    }
    if (stem_lower(exe) == "man") {
        utils::output::write(parser.help());
        return 0;
    }

    std::vector<std::string> extra(parsed.positionals.begin() + 1, parsed.positionals.end());
#ifdef _WIN32
    return run_help(exe, extra);
#else
    (void)extra;
    utils::output::writeln_err("man: not supported on this platform");
    return 1;
#endif
}
