#pragma once

#include <cstdint>
#include <string>

namespace utils {
namespace color {

enum class Color {
    Default,
    Black,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    White,
    BrightBlack,
    BrightRed,
    BrightGreen,
    BrightYellow,
    BrightBlue,
    BrightMagenta,
    BrightCyan,
    BrightWhite
};

// Named 16-color, 256-color index, or 24-bit RGB.
struct ColorSpec {
    enum class Kind { Default, Named, Indexed, Rgb };

    Kind kind = Kind::Default;
    Color named = Color::Default;
    int index = 0;
    std::uint8_t r = 0;
    std::uint8_t g = 0;
    std::uint8_t b = 0;

    ColorSpec() = default;
    ColorSpec(Color named_color);
};

ColorSpec rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b);
ColorSpec hex(const std::string& text);  // "#RGB" / "#RRGGBB" / "RRGGBB"
ColorSpec indexed(int index);            // 0-255

struct Style {
    ColorSpec fg;
    ColorSpec bg;
    bool bold = false;
};

// ANSI prefix for a style. Empty if the style has no effect.
std::string code(const Style& style);
std::string reset();

// Always wrap text with ANSI. Does not check TTY or NO_COLOR.
std::string paint(const std::string& text, const Style& style);
std::string paint(const std::string& text, const ColorSpec& fg);
std::string paint(const std::string& text, Color fg);

std::string black(const std::string& text);
std::string red(const std::string& text);
std::string green(const std::string& text);
std::string yellow(const std::string& text);
std::string blue(const std::string& text);
std::string magenta(const std::string& text);
std::string cyan(const std::string& text);
std::string white(const std::string& text);

}  // namespace color
}  // namespace utils
