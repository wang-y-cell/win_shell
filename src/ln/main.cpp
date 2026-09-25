#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <filesystem>
#include <system_error>

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("ln", "Create hard or symbolic links");
    parser.flag("s", "symbolic", "make symbolic links instead of hard links")
        .flag("f", "force", "remove existing destination files")
        .flag("", "help", "show this help")
        .positional("TARGET", "target and link name", true);

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

    const bool symbolic = parsed.has("symbolic");
    const bool force = parsed.has("force");
    auto paths = utils::fsutil::expand_globs(parsed.positionals);
    if (paths.size() < 2) {
        utils::output::writeln_err("ln: missing file operand");
        return 1;
    }

    const std::string dest_raw = paths.back();
    paths.pop_back();
    const auto dest = utils::sys::path_from_utf8(dest_raw);
    const bool dest_is_dir = utils::fsutil::is_dir(dest);
    if (paths.size() > 1 && !dest_is_dir) {
        utils::output::writeln_err("ln: target '" + dest_raw + "' is not a directory");
        return 1;
    }

    bool had_error = false;
    for (const auto& target_raw : paths) {
        const auto target = utils::sys::path_from_utf8(target_raw);
        auto link = dest_is_dir ? dest / target.filename() : dest;
        const std::string link_raw = dest_is_dir
                                         ? dest_raw + "\\" + utils::sys::path_to_utf8(target.filename())
                                         : dest_raw;

        if (!symbolic && !utils::fsutil::exists(target)) {
            utils::output::writeln_err("ln: failed to access '" + target_raw +
                                       "': No such file or directory");
            had_error = true;
            continue;
        }
        if (utils::fsutil::exists(link)) {
            if (!force) {
                utils::output::writeln_err("ln: failed to create link '" + link_raw +
                                           "': File exists");
                had_error = true;
                continue;
            }
            std::error_code rec;
            std::filesystem::remove(link, rec);
            if (rec) {
                utils::output::writeln_err("ln: cannot replace '" + link_raw + "': " + rec.message());
                had_error = true;
                continue;
            }
        }
        const auto parent = link.parent_path();
        if (!parent.empty() && !utils::fsutil::exists(parent)) {
            utils::output::writeln_err("ln: failed to create link '" + link_raw +
                                       "': No such file or directory");
            had_error = true;
            continue;
        }

        std::error_code ec;
        if (symbolic) {
            std::filesystem::create_symlink(target, link, ec);
        } else if (utils::fsutil::is_dir(target)) {
            utils::output::writeln_err("ln: '" + target_raw +
                                       "': hard link not allowed for directories (use -s)");
            had_error = true;
            continue;
        } else {
            std::filesystem::create_hard_link(target, link, ec);
        }
        if (ec) {
            utils::output::writeln_err("ln: failed to create link '" + link_raw + "': " +
                                       ec.message());
            had_error = true;
        }
    }
    return had_error ? 1 : 0;
}
