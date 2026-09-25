#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <cctype>

namespace {

std::string sniff(const std::string& name, const std::string& data, bool is_dir) {
    if (is_dir) {
        return "directory";
    }
    if (data.size() >= 2 && static_cast<unsigned char>(data[0]) == 0x4D &&
        static_cast<unsigned char>(data[1]) == 0x5A) {
        return "PE executable";
    }
    if (data.size() >= 4 && static_cast<unsigned char>(data[0]) == 0x7F && data[1] == 'E' &&
        data[2] == 'L' && data[3] == 'F') {
        return "ELF executable";
    }
    if (data.size() >= 4 && data[0] == 'P' && data[1] == 'K' && data[2] == 3 && data[3] == 4) {
        return "Zip archive";
    }
    if (data.size() >= 2 && static_cast<unsigned char>(data[0]) == 0x1F &&
        static_cast<unsigned char>(data[1]) == 0x8B) {
        return "gzip compressed data";
    }
    if (data.size() >= 8 && data.compare(0, 5, "%PDF-") == 0) {
        return "PDF document";
    }
    bool text = true;
    for (unsigned char c : data) {
        if (c == 0) {
            text = false;
            break;
        }
        if (c < 9 || (c > 13 && c < 32)) {
            text = false;
            break;
        }
    }
    if (text) {
        return "ASCII text";
    }
    const auto dot = name.find_last_of('.');
    if (dot != std::string::npos) {
        return name.substr(dot + 1) + " data";
    }
    return "data";
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("file", "Determine file type");
    parser.flag("b", "brief", "do not prepend filenames")
        .flag("", "help", "show this help")
        .positional("FILE", "file to inspect", true);

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
        utils::output::writeln_err("file: missing operand");
        return 1;
    }

    bool had_error = false;
    for (const auto& file : files) {
        const auto path = utils::sys::path_from_utf8(file);
        if (!utils::fsutil::exists(path)) {
            utils::output::writeln_err("file: " + file + ": No such file or directory");
            had_error = true;
            continue;
        }
        std::string data;
        std::string err;
        if (!utils::fsutil::is_dir(path)) {
            utils::sys::read_file_bytes(path, data, err);
            if (data.size() > 512) {
                data.resize(512);
            }
        }
        const std::string type = sniff(file, data, utils::fsutil::is_dir(path));
        if (parsed.has("brief")) {
            utils::output::writeln(type);
        } else {
            utils::output::writeln(file + ": " + type);
        }
    }
    return had_error ? 1 : 0;
}
