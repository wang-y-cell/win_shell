#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <fstream>
#include <memory>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
BOOL WINAPI tee_ignore_ctrl(DWORD) {
    return TRUE;
}
#endif

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("tee", "Read from stdin and write to files and stdout");
    parser.flag("a", "append", "append to the given files")
        .flag("i", "ignore-interrupts", "ignore interrupt signals")
        .flag("", "help", "show this help")
        .positional("FILE", "output file", true);

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

    const bool append = parsed.has("append");
    if (parsed.has("ignore-interrupts")) {
#ifdef _WIN32
        SetConsoleCtrlHandler(tee_ignore_ctrl, TRUE);
#endif
    }
    auto files = utils::fsutil::expand_globs(parsed.positionals);
    std::vector<std::unique_ptr<std::ofstream>> writers;
    writers.reserve(files.size());
    for (const auto& file : files) {
        const auto path = utils::sys::path_from_utf8(file);
        auto parent = path.parent_path();
        if (!parent.empty() && !utils::fsutil::exists(parent)) {
            utils::output::writeln_err("tee: " + file + ": No such file or directory");
            return 1;
        }
        auto out = std::make_unique<std::ofstream>();
        const auto mode = append ? std::ios::app : std::ios::trunc;
        out->open(path, std::ios::binary | mode);
        if (!*out) {
            utils::output::writeln_err("tee: " + file + ": cannot open");
            return 1;
        }
        writers.push_back(std::move(out));
    }

    for (const auto& line : utils::sys::read_stdin_lines()) {
        for (auto& w : writers) {
            *w << line << '\n';
        }
        utils::output::writeln(line);
    }
    return 0;
}
