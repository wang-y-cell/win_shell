#pragma once

#include "color.h"

#include <string>

namespace utils {
namespace output {

enum class ColorMode {
    Auto,
    Always,
    Never
};

// Enable VT sequences and UTF-8 console on Windows. Safe to call more than once.
void init();

void set_color_mode(ColorMode mode);
ColorMode color_mode();

// Parse "auto" / "always" / "never" (also yes/no/on/off). Returns false if unknown.
bool parse_color_mode(const std::string& text, ColorMode& mode);

bool is_stdout_tty();
bool is_stderr_tty();

// Visible columns of the stdout console. Falls back to 80 if unknown.
int terminal_width();

// Respects ColorMode, TTY, and NO_COLOR. stream "err" uses stderr for Auto.
bool color_enabled(bool stderr_stream = false);

void write(const std::string& text);
void write_err(const std::string& text);
void writeln(const std::string& text);
void writeln_err(const std::string& text);

// Apply color only when color_enabled() is true.
void write(const std::string& text, const color::Style& style);
void write(const std::string& text, const color::ColorSpec& fg);
void write(const std::string& text, color::Color fg);
void write_err(const std::string& text, const color::Style& style);
void write_err(const std::string& text, const color::ColorSpec& fg);
void write_err(const std::string& text, color::Color fg);
void writeln(const std::string& text, const color::Style& style);
void writeln(const std::string& text, const color::ColorSpec& fg);
void writeln(const std::string& text, color::Color fg);
void writeln_err(const std::string& text, const color::Style& style);
void writeln_err(const std::string& text, const color::ColorSpec& fg);
void writeln_err(const std::string& text, color::Color fg);

void flush();

}  // namespace output
}  // namespace utils
