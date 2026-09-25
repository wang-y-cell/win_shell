#pragma once

#include <cstddef>
#include <string>

namespace utils {
namespace width {

// Terminal columns for one Unicode code point: 0 / 1 (half-width) / 2 (full-width).
int codepoint_width(char32_t cp);

// Decode next UTF-8 code point. Returns false when index is at the end.
bool next_codepoint(const std::string& utf8, std::size_t& index, char32_t& cp);

// Terminal columns of a UTF-8 string. ANSI escapes do not count.
std::size_t display_width(const std::string& utf8);

// Pad with spaces to the given column count. Wider text is returned unchanged.
std::string pad_right(const std::string& utf8, std::size_t columns);
std::string pad_left(const std::string& utf8, std::size_t columns);

}  // namespace width
}  // namespace utils
