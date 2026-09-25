#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"
#include "utils/theme.h"

#include <algorithm>
#include <filesystem>
#include <system_error>

namespace fs = std::filesystem;

namespace {

struct Stats {
    int dirs = 0;
    int files = 0;
};

bool name_less(const fs::directory_entry& a, const fs::directory_entry& b) {
    const bool ad = a.is_directory();
    const bool bd = b.is_directory();
    if (ad != bd) {
        return ad && !bd;
    }
    return utils::fsutil::filename_utf8(a.path()) < utils::fsutil::filename_utf8(b.path());
}

void walk(const fs::path& dir, const std::string& prefix, int depth, int max_depth, bool show_all,
          bool dirs_only, Stats& stats) {
    if (depth >= max_depth) {
        return;
    }
    std::error_code ec;
    std::vector<fs::directory_entry> items;
    for (auto it = fs::directory_iterator(dir, fs::directory_options::skip_permission_denied, ec);
         it != fs::directory_iterator(); it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        const std::string name = utils::fsutil::filename_utf8(it->path());
        if (!show_all && utils::fsutil::name_starts_dot(name)) {
            continue;
        }
        if (dirs_only && !it->is_directory()) {
            continue;
        }
        items.push_back(*it);
    }
    std::sort(items.begin(), items.end(), name_less);

    for (std::size_t i = 0; i < items.size(); ++i) {
        const bool last = i + 1 == items.size();
        const std::string branch = last ? "`-- " : "|-- ";
        const std::string name = utils::fsutil::filename_utf8(items[i].path());
        const bool is_dir = items[i].is_directory();
        utils::output::write(prefix + branch);
        utils::output::writeln(name, utils::theme::item_color(name, is_dir));
        if (is_dir) {
            ++stats.dirs;
            walk(items[i].path(), prefix + (last ? "    " : "|   "), depth + 1, max_depth, show_all,
                 dirs_only, stats);
        } else {
            ++stats.files;
        }
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("tree", "List contents of directories in a tree-like format");
    parser.flag("a", "all", "print hidden files")
        .flag("d", "directory", "list directories only")
        .option("L", "level", "N", "descend only N directories deep")
        .flag("", "help", "show this help")
        .positional("DIR", "directory to list", true);

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

    const bool show_all = parsed.has("all");
    const bool dirs_only = parsed.has("directory");
    int max_depth = 100000;
    if (parsed.has("level")) {
        max_depth = parsed.get_int("level", 100000);
        if (max_depth < 0) {
            utils::output::writeln_err("tree: invalid level");
            return 1;
        }
    }

    auto paths = utils::fsutil::expand_globs(parsed.positionals);
    if (paths.empty()) {
        paths.emplace_back(".");
    }

    bool had_error = false;
    for (const auto& raw : paths) {
        const auto path = utils::sys::path_from_utf8(raw);
        if (!utils::fsutil::exists(path)) {
            utils::output::writeln_err("tree: " + raw + ": No such file or directory");
            had_error = true;
            continue;
        }
        if (!utils::fsutil::is_dir(path)) {
            utils::output::writeln_err("tree: " + raw + ": Not a directory");
            had_error = true;
            continue;
        }
        const auto display = utils::sys::path_to_utf8(std::filesystem::absolute(path));
        utils::output::writeln(display, utils::theme::blue());
        Stats stats;
        walk(path, "", 0, max_depth, show_all, dirs_only, stats);
        const char* dl = stats.dirs == 1 ? "directory" : "directories";
        const char* fl = stats.files == 1 ? "file" : "files";
        utils::output::writeln("");
        utils::output::writeln(std::to_string(stats.dirs) + " " + dl + ", " +
                               std::to_string(stats.files) + " " + fl);
    }
    return had_error ? 1 : 0;
}
