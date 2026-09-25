#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <filesystem>
#include <system_error>

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("readlink", "Print the value of a symbolic link");
    parser.flag("n", "no-newline", "do not output the trailing newline")
        .flag("f", "canonicalize", "canonicalize by following links and last component")
        .flag("", "help", "show this help")
        .positional("FILE", "symbolic link", true);

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
    auto files = utils::fsutil::expand_globs(parsed.positionals);
    if (files.empty()) {
        utils::output::writeln_err("readlink: missing operand");
        return 1;
    }

    bool had_error = false;
    for (const auto& file : files) {
        const auto path = utils::sys::path_from_utf8(file);
        std::error_code ec;
        const auto target = parsed.has("canonicalize") ? std::filesystem::weakly_canonical(path, ec)
                                                       : std::filesystem::read_symlink(path, ec);
        if (ec) {
            utils::output::writeln_err("readlink: " + file + ": " + ec.message());
            had_error = true;
            continue;
        }
        const auto text = utils::sys::path_to_utf8(target);
        if (parsed.has("no-newline")) {
            utils::output::write(text);
        } else {
            utils::output::writeln(text);
        }
    }
    return had_error ? 1 : 0;
}
