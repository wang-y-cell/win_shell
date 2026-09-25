#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <cstdlib>
#include <filesystem>
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

namespace {

std::vector<std::string> split_path(const std::string& path) {
    std::vector<std::string> parts;
    std::string cur;
    for (char c : path) {
        if (c == ';' || c == ':') {
#ifdef _WIN32
            if (c == ':') {
                cur.push_back(c);
                continue;
            }
#endif
            if (!cur.empty()) {
                parts.push_back(cur);
                cur.clear();
            }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) {
        parts.push_back(cur);
    }
    return parts;
}

#ifdef _WIN32
std::vector<std::string> pathexts() {
    const char* ext = std::getenv("PATHEXT");
    std::vector<std::string> out;
    if (ext == nullptr) {
        return {".EXE", ".BAT", ".CMD", ".COM"};
    }
    std::string cur;
    for (const char* p = ext; *p; ++p) {
        if (*p == ';') {
            if (!cur.empty()) {
                out.push_back(cur);
                cur.clear();
            }
        } else {
            cur.push_back(*p);
        }
    }
    if (!cur.empty()) {
        out.push_back(cur);
    }
    return out;
}
#endif

bool file_exists(const std::filesystem::path& path) {
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("which", "Locate a command");
    parser.flag("a", "all", "print all matching pathnames")
        .flag("", "help", "show this help")
        .positional("NAME", "command name", true);

    const auto args = utils::sys::utf8_argv(argc, argv);
    const auto parsed = parser.parse(args);
    if (!parsed.ok) {
        utils::output::writeln_err(parsed.error);
        return 1;
    }
    if (parsed.has("help")) {
        utils::output::write(parser.help());
        return 0;
    }

    const bool all = parsed.has("all");
    if (parsed.positionals.empty()) {
        utils::output::writeln_err("which: missing operand");
        return 1;
    }

    const char* path_env = std::getenv("PATH");
    const auto dirs = split_path(path_env ? path_env : "");
#ifdef _WIN32
    const auto exts = pathexts();
#endif

    bool had_error = false;
    for (const auto& name : parsed.positionals) {
        bool found = false;
        for (const auto& dir : dirs) {
            const auto base = utils::sys::path_from_utf8(dir) / utils::sys::path_from_utf8(name);
            std::vector<std::filesystem::path> candidates{base};
#ifdef _WIN32
            if (name.find('.') == std::string::npos) {
                for (const auto& ext : exts) {
                    candidates.push_back(utils::sys::path_from_utf8(dir) /
                                         utils::sys::path_from_utf8(name + ext));
                }
            }
#endif
            for (const auto& cand : candidates) {
                if (file_exists(cand)) {
                    utils::output::writeln(utils::sys::path_to_utf8(std::filesystem::absolute(cand)));
                    found = true;
                    if (!all) {
                        break;
                    }
                }
            }
            if (found && !all) {
                break;
            }
        }
        if (!found) {
            utils::output::writeln_err("which: no " + name + " in (" +
                                       (path_env ? path_env : "") + ")");
            had_error = true;
        }
    }
    return had_error ? 1 : 0;
}
