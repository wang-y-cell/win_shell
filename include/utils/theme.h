#pragma once

#include "color.h"

#include <cstdint>
#include <string>

namespace utils {
namespace theme {

color::ColorSpec green();
color::ColorSpec blue();
color::ColorSpec red();
color::ColorSpec purple();
color::ColorSpec white();
color::ColorSpec gray();
color::ColorSpec dark_gray();

color::ColorSpec item_color(const std::string& name, bool is_dir);
std::string format_size(std::uint64_t bytes, bool human, bool is_dir = false);

}  // namespace theme
}  // namespace utils
