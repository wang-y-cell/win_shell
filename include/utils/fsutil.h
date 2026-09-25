#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace utils {
namespace fsutil {

bool exists(const std::filesystem::path& path);
bool is_dir(const std::filesystem::path& path);
bool is_hidden(const std::filesystem::path& path, const std::string& name);
bool name_starts_dot(const std::string& name);

std::string filename_utf8(const std::filesystem::path& path);
std::vector<std::string> expand_globs(const std::vector<std::string>& patterns);

}  // namespace fsutil
}  // namespace utils
