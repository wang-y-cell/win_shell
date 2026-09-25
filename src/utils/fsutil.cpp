#include "../../include/utils/fsutil.h"

#include "../../include/utils/sys.h"

#include <algorithm>
#include <system_error>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace utils {
namespace fsutil {
namespace {

//判断字符串中是否包含通配符*,?
bool has_meta(const std::string& text) {
    return text.find_first_of("*?") != std::string::npos;
}

///判断文件名是否匹配通配符模式
///@param name 需要匹配的文件名
///@param pattern 通配符模式文件名
///@return 是否匹配
bool match_glob(const std::string& name, const std::string& pattern) {
    std::size_t ni = 0;
    std::size_t pi = 0;
    std::size_t star = std::string::npos;
    std::size_t star_n = 0;
    while (ni < name.size()) {
        if (pi < pattern.size() && (pattern[pi] == '?' || pattern[pi] == name[ni])) {
            ++ni;
            ++pi;
        } else if (pi < pattern.size() && pattern[pi] == '*') {
            star = pi++;
            star_n = ni;
        } else if (star != std::string::npos) {
            pi = star + 1;
            ni = ++star_n;
        } else {
            return false;
        }
    }
    while (pi < pattern.size() && pattern[pi] == '*') {
        ++pi;
    }
    return pi == pattern.size();
}

#ifdef _WIN32
///windows下判断两个文件名是否匹配通配符模式,不区分大小写
///@param name 需要匹配的文件名
///@param pattern 通配符模式文件名
///@return 是否匹配
bool match_glob_ci(const std::string& name, const std::string& pattern) {
    auto lower = [](std::string s) {
        for (char& c : s) {
            if (c >= 'A' && c <= 'Z') {
                c = static_cast<char>(c - 'A' + 'a');
            }
        }
        return s;
    };
    return match_glob(lower(name), lower(pattern));
}
#endif

}  // namespace

bool exists(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::exists(path, ec);
}

///判断文件路径是否为目录
bool is_dir(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::is_directory(path, ec);
}

bool name_starts_dot(const std::string& name) {
    return !name.empty() && name[0] == '.';
}

bool is_hidden(const std::filesystem::path& path, const std::string& name) {
    if (name_starts_dot(name)) {
        return true;
    }
#ifdef _WIN32
    const DWORD attr = GetFileAttributesW(path.c_str());
    return attr != INVALID_FILE_ATTRIBUTES && (attr & FILE_ATTRIBUTE_HIDDEN) != 0;
#else
    (void)path;
    return false;
#endif
}

//将文件路径的文件名转换为UTF-8字符串并放回
std::string filename_utf8(const std::filesystem::path& path) {
    return sys::path_to_utf8(path.filename());
}

///返回所有匹配得文件名列表,支持通配符*,?
///@param patterns 通配符模式列表
///@return 扩展后的文件名列表
std::vector<std::string> expand_globs(const std::vector<std::string>& patterns) {
    std::vector<std::string> out;
    for (const auto& pattern : patterns) {
        if (!has_meta(pattern)) { //如果字符串中不包含通配符*,?则直接添加到结果中
            out.push_back(pattern);
            continue;
        }

        //找到最后一个路径分割符
        const auto slash = pattern.find_last_of("\\/");
        //如果没有找到分割符,表示当前目录,否则截取父目录路径
        const std::string dir = slash == std::string::npos ? "." : pattern.substr(0, slash);
        //截取文件名
        const std::string file = slash == std::string::npos ? pattern : pattern.substr(slash + 1);
        const auto dir_path = sys::path_from_utf8(dir);

        std::error_code ec;
        std::filesystem::directory_iterator it(dir_path, ec);
        if (ec) {
            continue;
        }
        //遍历目录下的所有文件,我们需要知道当前文件是否匹配通配符模式
        for (const auto& entry : it) {
            //将文件路径转换为UTF-8字符串并返回 
            const std::string name = filename_utf8(entry.path());
#ifdef _WIN32
            //windows下文件名不区分大小写,所以需要使用match_glob_ci进行匹配
            if (match_glob_ci(name, file)) {
#else
            if (match_glob(name, file)) {
#endif
                if (dir == ".") {
                    out.push_back(name);
                } else {
                    // pattern.sub(slash,1) 表示截取从最后一个路径分割符到文件名的字符 '\'或'/'
                    out.push_back(dir + pattern.substr(slash, 1) + name);
                }
            }
        }
    }
    return out;
}

}  // namespace fsutil
}  // namespace utils
