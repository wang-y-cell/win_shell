#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace utils {
namespace sys {

std::string wide_to_utf8(const wchar_t* text, int length = -1);
std::wstring utf8_to_wide(const std::string& text);
std::string path_to_utf8(const std::filesystem::path& path);
std::filesystem::path path_from_utf8(const std::string& text);

std::vector<std::string> utf8_argv(int argc, char* argv[]);

bool stdin_is_tty();
bool confirm(const std::string& question);
std::vector<std::string> read_stdin_lines();
bool read_file_lines(const std::filesystem::path& path, std::vector<std::string>& lines,
                     std::string& error);
bool read_file_bytes(const std::filesystem::path& path, std::string& data, std::string& error);

}  // namespace sys
}  // namespace utils
