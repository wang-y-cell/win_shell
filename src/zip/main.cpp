#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace {

std::uint32_t crc32_of(const std::string& data) {
    std::uint32_t crc = 0xFFFFFFFFu;
    for (unsigned char c : data) {
        crc ^= c;
        for (int i = 0; i < 8; ++i) {
            crc = (crc >> 1) ^ (0xEDB88320u & (0u - (crc & 1u)));
        }
    }
    return ~crc;
}

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
struct Central {
    std::uint32_t sig;
    std::uint16_t made;
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
    std::uint16_t comment;
    std::uint16_t disk;
    std::uint16_t iattr;
    std::uint32_t eattr;
    std::uint32_t offset;
};
struct End {
    std::uint32_t sig;
    std::uint16_t disk;
    std::uint16_t cdisk;
    std::uint16_t entries;
    std::uint16_t total;
    std::uint32_t csize;
    std::uint32_t coffset;
    std::uint16_t comment;
};
#pragma pack(pop)

struct Entry {
    std::string name;
    std::string data;
    std::uint32_t crc = 0;
    std::uint32_t offset = 0;
};

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("zip", "Package files into a zip archive (stored)");
    parser.flag("", "help", "show this help")
        .positional("ARCHIVE", "zip file to create")
        .positional("FILE", "files to add", true);

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
    if (parsed.positionals.size() < 2) {
        utils::output::writeln_err("zip: missing operand");
        return 1;
    }

    const auto archive = utils::sys::path_from_utf8(parsed.positionals[0]);
    std::vector<Entry> entries;
    for (std::size_t i = 1; i < parsed.positionals.size(); ++i) {
        const auto files = utils::fsutil::expand_globs({parsed.positionals[i]});
        for (const auto& file : files) {
            const auto path = utils::sys::path_from_utf8(file);
            if (utils::fsutil::is_dir(path)) {
                continue;
            }
            Entry e;
            e.name = file;
            std::string err;
            if (!utils::sys::read_file_bytes(path, e.data, err)) {
                utils::output::writeln_err("zip: " + file + ": " + err);
                return 1;
            }
            e.crc = crc32_of(e.data);
            entries.push_back(std::move(e));
        }
    }

    std::ofstream out(archive, std::ios::binary);
    if (!out) {
        utils::output::writeln_err("zip: cannot write archive");
        return 1;
    }
    std::uint32_t offset = 0;
    for (auto& e : entries) {
        e.offset = offset;
        Local loc{};
        loc.sig = 0x04034b50;
        loc.ver = 20;
        loc.method = 0;
        loc.crc = e.crc;
        loc.csize = loc.usize = static_cast<std::uint32_t>(e.data.size());
        loc.namelen = static_cast<std::uint16_t>(e.name.size());
        out.write(reinterpret_cast<const char*>(&loc), sizeof(loc));
        out.write(e.name.data(), static_cast<std::streamsize>(e.name.size()));
        out.write(e.data.data(), static_cast<std::streamsize>(e.data.size()));
        offset += static_cast<std::uint32_t>(sizeof(loc) + e.name.size() + e.data.size());
        utils::output::writeln("  adding: " + e.name);
    }
    const std::uint32_t central_off = offset;
    for (const auto& e : entries) {
        Central c{};
        c.sig = 0x02014b50;
        c.made = c.ver = 20;
        c.method = 0;
        c.crc = e.crc;
        c.csize = c.usize = static_cast<std::uint32_t>(e.data.size());
        c.namelen = static_cast<std::uint16_t>(e.name.size());
        c.offset = e.offset;
        out.write(reinterpret_cast<const char*>(&c), sizeof(c));
        out.write(e.name.data(), static_cast<std::streamsize>(e.name.size()));
        offset += static_cast<std::uint32_t>(sizeof(c) + e.name.size());
    }
    End end{};
    end.sig = 0x06054b50;
    end.entries = end.total = static_cast<std::uint16_t>(entries.size());
    end.csize = offset - central_off;
    end.coffset = central_off;
    out.write(reinterpret_cast<const char*>(&end), sizeof(end));
    return 0;
}
