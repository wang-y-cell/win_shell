#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"
#include "utils/theme.h"

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <system_error>

namespace fs = std::filesystem;

namespace {

std::uint64_t dir_size(const fs::path& path, std::error_code& ec) {
    std::uint64_t sum = 0;
    for (auto it = fs::recursive_directory_iterator(
             path, fs::directory_options::skip_permission_denied, ec);
         it != fs::recursive_directory_iterator(); it.increment(ec)) {
        if (ec) {
            ec.clear();
            continue;
        }
        if (it->is_regular_file()) {
            sum += it->file_size(ec);
            if (ec) {
                ec.clear();
            }
        }
    }
    return sum;
}

void emit(std::uint64_t bytes, const std::string& label, bool human, int width) {
    const std::string size = utils::theme::format_size(bytes, human, false);
    std::ostringstream out;
    out << std::setw(width) << size << "  " << label;
    utils::output::writeln(out.str());
}

std::uint64_t walk(const fs::path& dir, const std::string& label, bool human, bool all, int width,
                   bool& had_error) {
    std::error_code ec;
    std::vector<fs::directory_entry> children;
    for (auto it = fs::directory_iterator(dir, fs::directory_options::skip_permission_denied, ec);
         it != fs::directory_iterator(); it.increment(ec)) {
        if (ec) {
            utils::output::writeln_err("du: cannot read directory '" + label + "': " + ec.message());
            had_error = true;
            ec.clear();
            break;
        }
        children.push_back(*it);
    }
    std::sort(children.begin(), children.end(), [](const auto& a, const auto& b) {
        return utils::fsutil::filename_utf8(a.path()) < utils::fsutil::filename_utf8(b.path());
    });

    for (const auto& child : children) {
        const std::string child_label = label + "\\" + utils::fsutil::filename_utf8(child.path());
        if (child.is_directory()) {
            walk(child.path(), child_label, human, all, width, had_error);
        } else if (all) {
            std::error_code fec;
            const auto size = child.is_regular_file() ? child.file_size(fec) : 0;
            emit(size, child_label, human, width);
        }
    }
    const auto bytes = dir_size(dir, ec);
    emit(bytes, label, human, width);
    return bytes;
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("du", "Estimate file space usage");
    parser.flag("h", "human-readable", "print sizes in human readable format")
        .flag("s", "summarize", "display only a total for each argument")
        .flag("a", "all", "write counts for all files, not just directories")
        .flag("", "help", "show this help")
        .positional("PATH", "file or directory", true);

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

    const bool human = parsed.has("human-readable");
    const bool summarize = parsed.has("summarize");
    const bool all = parsed.has("all");
    auto paths = utils::fsutil::expand_globs(parsed.positionals);
    if (paths.empty()) {
        paths.emplace_back(".");
    }
    const int width = human ? 8 : 12;

    bool had_error = false;
    for (const auto& raw : paths) {
        const auto path = utils::sys::path_from_utf8(raw);
        if (!utils::fsutil::exists(path)) {
            utils::output::writeln_err("du: cannot access '" + raw + "': No such file or directory");
            had_error = true;
            continue;
        }
        std::error_code ec;
        if (summarize || !utils::fsutil::is_dir(path)) {
            std::uint64_t bytes = 0;
            if (utils::fsutil::is_dir(path)) {
                bytes = dir_size(path, ec);
            } else {
                bytes = std::filesystem::file_size(path, ec);
            }
            emit(bytes, raw, human, width);
            continue;
        }
        walk(path, raw, human, all, width, had_error);
    }
    return had_error ? 1 : 0;
}
