#include "../../include/utils/width.h"

namespace utils {
namespace width {
namespace {

bool is_wide(char32_t cp) {
    return (cp >= 0x1100 && cp <= 0x115F) ||
           (cp >= 0x2329 && cp <= 0x232A) ||
           (cp >= 0x2E80 && cp <= 0xA4CF) ||
           (cp >= 0xAC00 && cp <= 0xD7A3) ||
           (cp >= 0xF900 && cp <= 0xFAFF) ||
           (cp >= 0xFE10 && cp <= 0xFE19) ||
           (cp >= 0xFE30 && cp <= 0xFE6F) ||
           (cp >= 0xFF00 && cp <= 0xFF60) ||
           (cp >= 0xFFE0 && cp <= 0xFFE6) ||
           (cp >= 0x20000 && cp <= 0x2FFFD) ||
           (cp >= 0x30000 && cp <= 0x3FFFD);
}

bool skip_ansi(const std::string& utf8, std::size_t& index) {
    if (index >= utf8.size()) {
        return false;
    }
    const unsigned char c = static_cast<unsigned char>(utf8[index]);
    if (c != 0x1B) {
        return false;
    }
    ++index;
    if (index >= utf8.size()) {
        return true;
    }
    const unsigned char next = static_cast<unsigned char>(utf8[index]);
    if (next == '[') {
        ++index;
        while (index < utf8.size()) {
            const unsigned char ch = static_cast<unsigned char>(utf8[index]);
            ++index;
            if (ch >= 0x40 && ch <= 0x7E) {
                break;
            }
        }
        return true;
    }
    if (next == ']') {
        ++index;
        while (index < utf8.size()) {
            const unsigned char ch = static_cast<unsigned char>(utf8[index]);
            ++index;
            if (ch == 0x07) {
                break;
            }
            if (ch == 0x1B && index < utf8.size() && utf8[index] == '\\') {
                ++index;
                break;
            }
        }
        return true;
    }
    ++index;
    return true;
}

}  // namespace

int codepoint_width(char32_t cp) {
    if (cp == 0 || cp < 0x20 || cp == 0x7F) {
        return 0;
    }
    if (cp < 0x7F) {
        return 1;
    }
    if (is_wide(cp)) {
        return 2;
    }
    return 1;
}

bool next_codepoint(const std::string& utf8, std::size_t& index, char32_t& cp) {
    if (index >= utf8.size()) {
        return false;
    }

    const unsigned char lead = static_cast<unsigned char>(utf8[index]);
    if (lead < 0x80) {
        cp = lead;
        ++index;
        return true;
    }

    int extra = 0;
    char32_t min_cp = 0;
    if ((lead & 0xE0) == 0xC0) {
        extra = 1;
        cp = lead & 0x1F;
        min_cp = 0x80;
    } else if ((lead & 0xF0) == 0xE0) {
        extra = 2;
        cp = lead & 0x0F;
        min_cp = 0x800;
    } else if ((lead & 0xF8) == 0xF0) {
        extra = 3;
        cp = lead & 0x07;
        min_cp = 0x10000;
    } else {
        cp = 0xFFFD;
        ++index;
        return true;
    }

    if (index + static_cast<std::size_t>(extra) >= utf8.size()) {
        cp = 0xFFFD;
        index = utf8.size();
        return true;
    }

    for (int i = 1; i <= extra; ++i) {
        const unsigned char unit = static_cast<unsigned char>(utf8[index + i]);
        if ((unit & 0xC0) != 0x80) {
            cp = 0xFFFD;
            ++index;
            return true;
        }
        cp = (cp << 6) | (unit & 0x3F);
    }
    index += static_cast<std::size_t>(extra) + 1;

    if (cp < min_cp || (cp >= 0xD800 && cp <= 0xDFFF) || cp > 0x10FFFF) {
        cp = 0xFFFD;
    }
    return true;
}

std::size_t display_width(const std::string& utf8) {
    std::size_t columns = 0;
    std::size_t index = 0;
    while (index < utf8.size()) {
        if (skip_ansi(utf8, index)) {
            continue;
        }
        char32_t cp = 0;
        if (!next_codepoint(utf8, index, cp)) {
            break;
        }
        const int w = codepoint_width(cp);
        if (w > 0) {
            columns += static_cast<std::size_t>(w);
        }
    }
    return columns;
}

std::string pad_right(const std::string& utf8, std::size_t columns) {
    const std::size_t w = display_width(utf8);
    if (w >= columns) {
        return utf8;
    }
    return utf8 + std::string(columns - w, ' ');
}

std::string pad_left(const std::string& utf8, std::size_t columns) {
    const std::size_t w = display_width(utf8);
    if (w >= columns) {
        return utf8;
    }
    return std::string(columns - w, ' ') + utf8;
}

}  // namespace width
}  // namespace utils
