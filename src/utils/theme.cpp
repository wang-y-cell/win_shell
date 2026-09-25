#include "../../include/utils/theme.h"

#include <cstdio>

namespace utils {
namespace theme {

color::ColorSpec green() { return color::rgb(152, 195, 121); }
color::ColorSpec blue() { return color::rgb(97, 175, 239); }
color::ColorSpec red() { return color::rgb(224, 108, 117); }
color::ColorSpec purple() { return color::rgb(255, 85, 255); }
color::ColorSpec white() { return color::rgb(220, 223, 228); }
color::ColorSpec gray() { return color::rgb(192, 192, 192); }
color::ColorSpec dark_gray() { return color::rgb(128, 128, 128); }

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

color::ColorSpec item_color(const std::string& name, bool is_dir) {
    if (is_dir) {
        return blue();
    }
    const std::string ext = extension_of(name);
    if (ext == ".exe" || ext == ".bat" || ext == ".ps1") {
        return green();
    }
    if (ext == ".zip" || ext == ".7z" || ext == ".rar" || ext == ".tar" || ext == ".gz") {
        return red();
    }
    if (ext == ".jpg" || ext == ".png" || ext == ".gif") {
        return purple();
    }
    return white();
}

std::string format_size(std::uint64_t bytes, bool human, bool is_dir) {
    if (is_dir) {
        return "-";
    }
    if (!human) {
        return std::to_string(bytes);
    }
    static const char* units[] = {"B", "K", "M", "G", "T", "P"};
    double size = static_cast<double>(bytes);
    int unit = 0;
    while (size >= 1024.0 && unit < 5) {
        size /= 1024.0;
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

}  // namespace theme
}  // namespace utils
