#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <filesystem>
#include <system_error>

namespace {

struct CopyFlags {
    bool force = false;
    bool interactive = false;
    bool no_clobber = false;
    bool preserve = false;
    bool verbose = false;
};

void copy_error(const std::filesystem::path& src, const std::filesystem::path& target,
                const std::string& message) {
    utils::output::writeln_err("cp: cannot copy '" + utils::sys::path_to_utf8(src) + "' to '" +
                               utils::sys::path_to_utf8(target) + "': " + message);
}

void preserve_time(const std::filesystem::path& src, const std::filesystem::path& target) {
    std::error_code ec;
    const auto t = std::filesystem::last_write_time(src, ec);
    if (!ec) {
        std::filesystem::last_write_time(target, t, ec);
    }
}

void log_copy(const std::filesystem::path& src, const std::filesystem::path& target, bool verbose) {
    if (!verbose) {
        return;
    }
    utils::output::writeln("'" + utils::sys::path_to_utf8(src) + "' -> '" +
                           utils::sys::path_to_utf8(target) + "'");
}

bool allow_overwrite(const std::filesystem::path& src, const std::filesystem::path& target,
                     const CopyFlags& flags, bool& had_error) {
    std::error_code ec;
    if (!std::filesystem::exists(target, ec)) {
        return true;
    }
    if (flags.no_clobber) {
        return false;
    }
    if (flags.interactive) {
        return utils::sys::confirm("cp: overwrite '" + utils::sys::path_to_utf8(target) + "'? ");
    }
    if (!flags.force) {
        copy_error(src, target, "File exists");
        had_error = true;
        return false;
    }
    return true;
}

void copy_recursive(const std::filesystem::path& src, const std::filesystem::path& target,
                    const CopyFlags& flags, bool& had_error) {
    if (utils::fsutil::is_dir(src)) {
        std::error_code ec;
        if (std::filesystem::exists(target, ec) && !utils::fsutil::is_dir(target)) {
            copy_error(src, target, "File exists");
            had_error = true;
            return;
        }
        if (!utils::fsutil::is_dir(target)) {
            std::filesystem::create_directory(target, ec);
            if (ec) {
                copy_error(src, target, ec.message());
                had_error = true;
                return;
            }
        }
        log_copy(src, target, flags.verbose);

        std::error_code it_ec;
        for (std::filesystem::directory_iterator it(src, it_ec);
             !it_ec && it != std::filesystem::directory_iterator(); it.increment(it_ec)) {
            const auto child = it->path();
            copy_recursive(child, target / child.filename(), flags, had_error);
        }
        if (it_ec) {
            copy_error(src, target, it_ec.message());
            had_error = true;
        }
        if (flags.preserve) {
            preserve_time(src, target);
        }
        return;
    }

    if (!allow_overwrite(src, target, flags, had_error)) {
        return;
    }
    const auto options = flags.force ? std::filesystem::copy_options::overwrite_existing
                                     : std::filesystem::copy_options::none;
    std::error_code ec;
    std::filesystem::copy_file(src, target, options, ec);
    if (ec) {
        copy_error(src, target, ec.message());
        had_error = true;
        return;
    }
    if (flags.preserve) {
        preserve_time(src, target);
    }
    log_copy(src, target, flags.verbose);
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("cp", "Copy files and directories");
    parser.flag("r", "recursive", "copy directories recursively")
        .flag("R", "RECURSIVE", "same as -r")
        .flag("f", "force", "overwrite destination if it exists")
        .flag("i", "interactive", "prompt before overwrite")
        .flag("n", "no-clobber", "do not overwrite an existing file")
        .flag("p", "preserve", "preserve timestamps")
        .flag("v", "verbose", "explain what is being done")
        .flag("", "help", "show this help")
        .positional("FILE", "source and destination", true);

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

    const bool recursive = parsed.has("recursive") || parsed.has("RECURSIVE");
    const CopyFlags flags{
        parsed.has("force") && !parsed.has("no-clobber"),
        parsed.has("interactive") && !parsed.has("no-clobber"),
        parsed.has("no-clobber"),
        parsed.has("preserve"),
        parsed.has("verbose"),
    };
    //扩展通配符模式
    auto paths = utils::fsutil::expand_globs(parsed.positionals);
    if (paths.size() < 2) {
        utils::output::writeln_err("cp: missing file operand");
        return 1;
    }

    const std::string dest_raw = paths.back();
    paths.pop_back();
    const auto dest = utils::sys::path_from_utf8(dest_raw);
    //如果目标文件不是目录,并且有多个源文件,则报错
    if (paths.size() > 1 && !utils::fsutil::is_dir(dest)) {
        utils::output::writeln_err("cp: target '" + dest_raw + "' is not a directory");
        return 1;
    }

    bool had_error = false;
    for (const auto& src_raw : paths) {
        //src是要复制得源文件路径
        const auto src = utils::sys::path_from_utf8(src_raw);
        //如果这个文件不存在,则报错
        if (!utils::fsutil::exists(src)) {
            utils::output::writeln_err("cp: cannot stat '" + src_raw +
                                       "': No such file or directory");
            had_error = true;
            continue;
        }
        //如果源文件是目录,并且没有递归选项,则报错
        if (utils::fsutil::is_dir(src) && !recursive) {
            utils::output::writeln_err("cp: -r not specified; omitting directory '" + src_raw + "'");
            had_error = true;
            continue;
        }

        auto target = dest;
        //如果目标文件是目录,则将源文件的文件名添加到目标文件路径中
        if (utils::fsutil::is_dir(dest)) {
            target = dest / src.filename();
        }
        copy_recursive(src, target, flags, had_error);
    }
    return had_error ? 1 : 0;
}
