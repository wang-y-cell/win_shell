#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>

namespace {

#pragma pack(push, 1)
struct Local {
    std::uint32_t sig;
    std::uint16_t ver;
    std::uint16_t flag;
    std::uint16_t method;
    std::uint16_t time;
    std::uint16_t date;
    std::uint32_t crc;
    std::uint32_t csize;
    std::uint32_t usize;
    std::uint16_t namelen;
    std::uint16_t extralen;
};
#pragma pack(pop)

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("unzip", "Extract files from a zip archive");
    parser.flag("l", "list", "list archive contents")
        .flag("", "help", "show this help")
        .positional("ARCHIVE", "zip file")
        .positional("FILE", "members to extract", true);

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
    if (parsed.positionals.empty()) {
        utils::output::writeln_err("unzip: missing archive");
        return 1;
    }

    std::string data;
    std::string err;
    if (!utils::sys::read_file_bytes(utils::sys::path_from_utf8(parsed.positionals[0]), data, err)) {
        utils::output::writeln_err("unzip: " + err);
        return 1;
    }

    const bool list = parsed.has("list");
    std::size_t off = 0;
    while (off + sizeof(Local) <= data.size()) {
        Local loc{};
        std::memcpy(&loc, data.data() + off, sizeof(loc));
        if (loc.sig != 0x04034b50) {
            break;
        }
        off += sizeof(loc);
        if (off + loc.namelen > data.size()) {
            break;
        }
        const std::string name(data.data() + off, loc.namelen);
        off += loc.namelen + loc.extralen;
        if (off + loc.csize > data.size()) {
            break;
        }
        if (list) {
            utils::output::writeln(std::to_string(loc.usize) + "  " + name);
        } else if (loc.method == 0) {
            const auto path = utils::sys::path_from_utf8(name);
            const auto parent = path.parent_path();
            if (!parent.empty()) {
                std::filesystem::create_directories(parent);
            }
            std::ofstream out(path, std::ios::binary);
            out.write(data.data() + off, static_cast<std::streamsize>(loc.csize));
            utils::output::writeln("  inflating: " + name);
        } else {
            utils::output::writeln_err("unzip: " + name + ": compressed entries not supported");
        }
        off += loc.csize;
        if (loc.flag & 0x8) {
            off += 12;
        }
    }
    return 0;
}
