#include "../../include/utils/sys.h"

#include <fstream>
#include <iostream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <io.h>
#include <windows.h>
#include <shellapi.h>
#define UTILS_ISATTY(fd) _isatty(fd)
#define UTILS_FILENO(file) _fileno(file)
#else
#include <unistd.h>
#define UTILS_ISATTY(fd) isatty(fd)
#define UTILS_FILENO(file) fileno(file)
#endif

namespace utils {
namespace sys {

std::string wide_to_utf8(const wchar_t* text, int length) {
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

std::wstring utf8_to_wide(const std::string& text) {
#ifdef _WIN32
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
#else
    (void)text;
    return {};
#endif
}

std::string path_to_utf8(const std::filesystem::path& path) {
#ifdef _WIN32
    const auto& w = path.native();
    return wide_to_utf8(w.c_str(), static_cast<int>(w.size()));
#else
    return path.u8string();
#endif
}

std::filesystem::path path_from_utf8(const std::string& text) {
#ifdef _WIN32
    return std::filesystem::path(utf8_to_wide(text));
#else
    return std::filesystem::path(text);
#endif
}

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

bool stdin_is_tty() {
    return UTILS_ISATTY(UTILS_FILENO(stdin)) != 0;
}

std::vector<std::string> read_stdin_lines() {
    std::vector<std::string> lines;
    std::string line;
    while (std::getline(std::cin, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(std::move(line));
    }
    return lines;
}

bool read_file_lines(const std::filesystem::path& path, std::vector<std::string>& lines,
                     std::string& error) {
    std::ifstream in;
    in.open(path, std::ios::binary);
    if (!in) {
        error = "cannot open file";
        return false;
    }
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') {
            line.pop_back();
        }
        lines.push_back(std::move(line));
    }
    return true;
}

}  // namespace sys
}  // namespace utils
