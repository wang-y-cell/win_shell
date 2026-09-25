#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <random>
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

std::string random_name(std::size_t n) {
    static const char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789";
    std::mt19937 rng{std::random_device{}()};
    std::uniform_int_distribution<int> dist(0, 61);
    std::string out;
    for (std::size_t i = 0; i < n; ++i) {
        out.push_back(alphabet[dist(rng)]);
    }
    return out;
}

std::string fill_template(std::string tmpl) {
    const auto pos = tmpl.find("XXXXXX");
    if (pos == std::string::npos) {
        tmpl += "XXXXXX";
        return fill_template(tmpl);
    }
    tmpl.replace(pos, 6, random_name(6));
    return tmpl;
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("mktemp", "Create a temporary file or directory");
    parser.flag("d", "directory", "create a directory, not a file")
        .flag("u", "dry-run", "do not create anything; merely print a name")
        .option("p", "tmpdir", "DIR", "interpret TEMPLATE relative to DIR")
        .flag("", "help", "show this help")
        .positional("TEMPLATE", "template ending in XXXXXX");

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

    std::string tmpl = parsed.positionals.empty() ? "tmp.XXXXXX" : parsed.positionals[0];
    std::filesystem::path dir;
    if (parsed.has("tmpdir")) {
        dir = utils::sys::path_from_utf8(parsed.get("tmpdir"));
    } else {
#ifdef _WIN32
        wchar_t buf[MAX_PATH];
        const DWORD n = GetTempPathW(MAX_PATH, buf);
        dir = n ? std::filesystem::path(std::wstring(buf, n)) : std::filesystem::temp_directory_path();
#else
        dir = std::filesystem::temp_directory_path();
#endif
    }

    for (int attempt = 0; attempt < 100; ++attempt) {
        const auto name = fill_template(tmpl);
        const auto path = std::filesystem::path(name).is_absolute()
                              ? utils::sys::path_from_utf8(name)
                              : dir / utils::sys::path_from_utf8(name);
        if (parsed.has("dry-run")) {
            utils::output::writeln(utils::sys::path_to_utf8(path));
            return 0;
        }
        std::error_code ec;
        if (std::filesystem::exists(path, ec)) {
            continue;
        }
        if (parsed.has("directory")) {
            if (!std::filesystem::create_directory(path, ec) || ec) {
                continue;
            }
        } else {
            std::ofstream out(path, std::ios::binary);
            if (!out) {
                continue;
            }
        }
        utils::output::writeln(utils::sys::path_to_utf8(path));
        return 0;
    }
    utils::output::writeln_err("mktemp: failed to create file");
    return 1;
}
