#include "../../include/utils/color.h"

#include <sstream>

namespace utils {
namespace color {
namespace {

int fg_code(Color color) {
    switch (color) {
        case Color::Black:
            return 30;
        case Color::Red:
            return 31;
        case Color::Green:
            return 32;
        case Color::Yellow:
            return 33;
        case Color::Blue:
            return 34;
        case Color::Magenta:
            return 35;
        case Color::Cyan:
            return 36;
        case Color::White:
            return 37;
        case Color::BrightBlack:
            return 90;
        case Color::BrightRed:
            return 91;
        case Color::BrightGreen:
            return 92;
        case Color::BrightYellow:
            return 93;
        case Color::BrightBlue:
            return 94;
        case Color::BrightMagenta:
            return 95;
        case Color::BrightCyan:
            return 96;
        case Color::BrightWhite:
            return 97;
        case Color::Default:
        default:
            return -1;
    }
}

int hex_digit(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

bool parse_hex_byte(const std::string& text, std::size_t i, std::uint8_t& out) {
    const int hi = hex_digit(text[i]);
    const int lo = hex_digit(text[i + 1]);
    if (hi < 0 || lo < 0) {
        return false;
    }
    out = static_cast<std::uint8_t>((hi << 4) | lo);
    return true;
}

bool is_default(const ColorSpec& spec) {
    return spec.kind == ColorSpec::Kind::Default ||
           (spec.kind == ColorSpec::Kind::Named && spec.named == Color::Default);
}

void append_sgr(std::ostringstream& out, bool& first, int value) {
    if (!first) {
        out << ';';
    }
    first = false;
    out << value;
}

void append_color(std::ostringstream& out, bool& first, const ColorSpec& spec, bool background) {
    if (is_default(spec)) {
        return;
    }
    if (spec.kind == ColorSpec::Kind::Rgb) {
        append_sgr(out, first, background ? 48 : 38);
        append_sgr(out, first, 2);
        append_sgr(out, first, spec.r);
        append_sgr(out, first, spec.g);
        append_sgr(out, first, spec.b);
        return;
    }
    if (spec.kind == ColorSpec::Kind::Indexed) {
        append_sgr(out, first, background ? 48 : 38);
        append_sgr(out, first, 5);
        append_sgr(out, first, spec.index);
        return;
    }
    const int named = fg_code(spec.named);
    if (named >= 0) {
        append_sgr(out, first, background ? named + 10 : named);
    }
}

}  // namespace

ColorSpec::ColorSpec(Color named_color)
    : kind(named_color == Color::Default ? Kind::Default : Kind::Named),
      named(named_color) {}

ColorSpec rgb(std::uint8_t r, std::uint8_t g, std::uint8_t b) {
    ColorSpec spec;
    spec.kind = ColorSpec::Kind::Rgb;
    spec.r = r;
    spec.g = g;
    spec.b = b;
    return spec;
}

ColorSpec hex(const std::string& text) {
    std::string body = text;
    if (!body.empty() && body[0] == '#') {
        body.erase(0, 1);
    }

    ColorSpec spec;
    spec.kind = ColorSpec::Kind::Rgb;
    if (body.size() == 3) {
        const int r = hex_digit(body[0]);
        const int g = hex_digit(body[1]);
        const int b = hex_digit(body[2]);
        if (r < 0 || g < 0 || b < 0) {
            return {};
        }
        spec.r = static_cast<std::uint8_t>(r * 17);
        spec.g = static_cast<std::uint8_t>(g * 17);
        spec.b = static_cast<std::uint8_t>(b * 17);
        return spec;
    }
    if (body.size() == 6 && parse_hex_byte(body, 0, spec.r) &&
        parse_hex_byte(body, 2, spec.g) && parse_hex_byte(body, 4, spec.b)) {
        return spec;
    }
    return {};
}

ColorSpec indexed(int index) {
    ColorSpec spec;
    spec.kind = ColorSpec::Kind::Indexed;
    if (index < 0) {
        spec.index = 0;
    } else if (index > 255) {
        spec.index = 255;
    } else {
        spec.index = index;
    }
    return spec;
}

std::string code(const Style& style) {
    if (is_default(style.fg) && is_default(style.bg) && !style.bold) {
        return {};
    }

    std::ostringstream out;
    out << "\033[";
    bool first = true;
    if (style.bold) {
        append_sgr(out, first, 1);
    }
    append_color(out, first, style.fg, false);
    append_color(out, first, style.bg, true);
    out << 'm';
    return out.str();
}

std::string reset() {
    return "\033[0m";
}

std::string paint(const std::string& text, const Style& style) {
    if (text.empty()) {
        return {};
    }
    const std::string prefix = code(style);
    if (prefix.empty()) {
        return text;
    }
    return prefix + text + reset();
}

std::string paint(const std::string& text, const ColorSpec& fg) {
    return paint(text, Style{fg, ColorSpec{}, false});
}

std::string paint(const std::string& text, Color fg) {
    return paint(text, ColorSpec{fg});
}

std::string black(const std::string& text) { return paint(text, Color::Black); }
std::string red(const std::string& text) { return paint(text, Color::Red); }
std::string green(const std::string& text) { return paint(text, Color::Green); }
std::string yellow(const std::string& text) { return paint(text, Color::Yellow); }
std::string blue(const std::string& text) { return paint(text, Color::Blue); }
std::string magenta(const std::string& text) { return paint(text, Color::Magenta); }
std::string cyan(const std::string& text) { return paint(text, Color::Cyan); }
std::string white(const std::string& text) { return paint(text, Color::White); }

}  // namespace color
}  // namespace utils
