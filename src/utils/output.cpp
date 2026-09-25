#include "../../include/utils/output.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iostream>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define UTILS_ISATTY(fd) _isatty(fd)
#define UTILS_FILENO(file) _fileno(file)
#else
#include <unistd.h>
#define UTILS_ISATTY(fd) isatty(fd)
#define UTILS_FILENO(file) fileno(file)
#endif

namespace utils {
namespace output {
namespace {

ColorMode g_mode = ColorMode::Auto;
bool g_inited = false;

bool no_color_set() {
    const char* value = std::getenv("NO_COLOR");
    return value != nullptr && value[0] != '\0';
}

bool is_tty_file(FILE* file) {
    if (file == nullptr) {
        return false;
    }
    return UTILS_ISATTY(UTILS_FILENO(file)) != 0;
}

std::string to_lower(std::string text) {
    for (char& c : text) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

#ifdef _WIN32
/*
在 Windows 控制台上打开 VT（Virtual Terminal）处理，让控制台能识别 ANSI 转义序列，从而正常显示颜色、样式
Linux / macOS 终端默认就支持 ANSI，例如 \033[31m 变红、\033[0m 复位。
Windows 控制台默认不解释这些序列，直接当普通字符打印出来。Windows 10 起可以给控制台加上 ENABLE_VIRTUAL_TERMINAL_PROCESSING，行为才和 Unix 终端对齐。

这个文件后面的彩色输出（color::paint、maybe_paint）就是靠这类转义序列工作的，所以初始化时必须先打开 VT。
*/
void enable_vt(DWORD handle_id) {
    HANDLE handle = GetStdHandle(handle_id);
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE) {
        return;
    }
    DWORD mode = 0;
    if (!GetConsoleMode(handle, &mode)) {
        return;
    }
    SetConsoleMode(handle, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}
#endif

std::string maybe_paint(const std::string& text,
                        const color::Style& style,
                        bool stderr_stream) {
    if (!color_enabled(stderr_stream)) {
        return text;
    }
    return color::paint(text, style);
}

}  // namespace

void init() {
    if (g_inited) {
        return;
    }
#ifdef _WIN32
    enable_vt(STD_OUTPUT_HANDLE);
    enable_vt(STD_ERROR_HANDLE);
    SetConsoleOutputCP(CP_UTF8); //设置控制台输出代码点为UTF-8
    SetConsoleCP(CP_UTF8); //设置控制台输入代码点为UTF-8
#endif
    g_inited = true;
}

void set_color_mode(ColorMode mode) {
    g_mode = mode;
}

ColorMode color_mode() {
    return g_mode;
}

bool parse_color_mode(const std::string& text, ColorMode& mode) {
    const std::string key = to_lower(text);
    if (key == "auto") {
        mode = ColorMode::Auto;
        return true;
    }
    if (key == "always" || key == "yes" || key == "on" || key == "force") {
        mode = ColorMode::Always;
        return true;
    }
    if (key == "never" || key == "no" || key == "off" || key == "none") {
        mode = ColorMode::Never;
        return true;
    }
    return false;
}

bool is_stdout_tty() {
    return is_tty_file(stdout);
}

bool is_stderr_tty() {
    return is_tty_file(stderr);
}

bool color_enabled(bool stderr_stream) {
    if (no_color_set() || g_mode == ColorMode::Never) {
        return false;
    }
    if (g_mode == ColorMode::Always) {
        return true;
    }
    return stderr_stream ? is_stderr_tty() : is_stdout_tty();
}

void write(const std::string& text) {
    std::cout << text;
}

void write_err(const std::string& text) {
    std::cerr << text;
}

void writeln(const std::string& text) {
    std::cout << text << '\n';
}

void writeln_err(const std::string& text) {
    std::cerr << text << '\n';
}

void write(const std::string& text, const color::Style& style) {
    write(maybe_paint(text, style, false));
}

void write(const std::string& text, const color::ColorSpec& fg) {
    write(text, color::Style{fg, color::ColorSpec{}, false});
}

void write(const std::string& text, color::Color fg) {
    write(text, color::ColorSpec{fg});
}

void write_err(const std::string& text, const color::Style& style) {
    write_err(maybe_paint(text, style, true));
}

void write_err(const std::string& text, const color::ColorSpec& fg) {
    write_err(text, color::Style{fg, color::ColorSpec{}, false});
}

void write_err(const std::string& text, color::Color fg) {
    write_err(text, color::ColorSpec{fg});
}

void writeln(const std::string& text, const color::Style& style) {
    writeln(maybe_paint(text, style, false));
}

void writeln(const std::string& text, const color::ColorSpec& fg) {
    writeln(text, color::Style{fg, color::ColorSpec{}, false});
}

void writeln(const std::string& text, color::Color fg) {
    writeln(text, color::ColorSpec{fg});
}

void writeln_err(const std::string& text, const color::Style& style) {
    writeln_err(maybe_paint(text, style, true));
}

void writeln_err(const std::string& text, const color::ColorSpec& fg) {
    writeln_err(text, color::Style{fg, color::ColorSpec{}, false});
}

void writeln_err(const std::string& text, color::Color fg) {
    writeln_err(text, color::ColorSpec{fg});
}

void flush() {
    std::cout.flush();
    std::cerr.flush();
}

}  // namespace output
}  // namespace utils
