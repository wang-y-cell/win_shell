#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <system_error>

namespace fs = std::filesystem;

namespace {

#pragma pack(push, 1)
struct Ustar {
    char name[100];
    char mode[8];
    char uid[8];
    char gid[8];
    char size[12];
    char mtime[12];
    char chksum[8];
    char typeflag;
    char linkname[100];
    char magic[6];
    char version[2];
    char uname[32];
    char gname[32];
    char devmajor[8];
    char devminor[8];
    char prefix[155];
    char pad[12];
};
#pragma pack(pop)

static_assert(sizeof(Ustar) == 512, "ustar header");

void put_octal(char* dest, std::size_t n, std::uint64_t value) {
    std::snprintf(dest, n, "%0*llo", static_cast<int>(n - 1),
                  static_cast<unsigned long long>(value));
}

std::uint64_t get_octal(const char* src, std::size_t n) {
    std::uint64_t v = 0;
    for (std::size_t i = 0; i < n && src[i]; ++i) {
        if (src[i] >= '0' && src[i] <= '7') {
            v = (v << 3) | static_cast<std::uint64_t>(src[i] - '0');
        }
    }
    return v;
}

void checksum(Ustar& h) {
    std::memset(h.chksum, ' ', 8);
    unsigned int sum = 0;
    const auto* p = reinterpret_cast<const unsigned char*>(&h);
    for (std::size_t i = 0; i < sizeof(h); ++i) {
        sum += p[i];
    }
    std::snprintf(h.chksum, 8, "%06o", sum);
    h.chksum[6] = '\0';
    h.chksum[7] = ' ';
}

void write_blocks(std::ostream& out, const std::string& data) {
    out.write(data.data(), static_cast<std::streamsize>(data.size()));
    const std::size_t pad = (512 - (data.size() % 512)) % 512;
    if (pad) {
        std::string z(pad, '\0');
        out.write(z.data(), static_cast<std::streamsize>(pad));
    }
}

bool add_file(std::ostream& out, const fs::path& path, const std::string& name, bool verbose) {
    Ustar h{};
    std::strncpy(h.name, name.c_str(), 99);
    std::memcpy(h.magic, "ustar", 5);
    h.magic[5] = '\0';
    h.version[0] = '0';
    h.version[1] = '0';
    put_octal(h.mode, 8, 0644);
    put_octal(h.uid, 8, 0);
    put_octal(h.gid, 8, 0);
    if (fs::is_directory(path)) {
        h.typeflag = '5';
        put_octal(h.size, 12, 0);
        checksum(h);
        out.write(reinterpret_cast<const char*>(&h), 512);
        if (verbose) {
            utils::output::writeln(name);
        }
        std::error_code ec;
        for (auto it = fs::directory_iterator(path, ec); it != fs::directory_iterator();
             it.increment(ec)) {
            const std::string child = name + (name.empty() || name.back() == '/' ? "" : "/") +
                                      utils::fsutil::filename_utf8(it->path());
            if (!add_file(out, it->path(), child, verbose)) {
                return false;
            }
        }
        return true;
    }
    h.typeflag = '0';
    std::string data;
    std::string err;
    if (!utils::sys::read_file_bytes(path, data, err)) {
        utils::output::writeln_err("tar: " + name + ": " + err);
        return false;
    }
    put_octal(h.size, 12, data.size());
    checksum(h);
    out.write(reinterpret_cast<const char*>(&h), 512);
    write_blocks(out, data);
    if (verbose) {
        utils::output::writeln(name);
    }
    return true;
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("tar", "Tape archive");
    parser.flag("c", "create", "create a new archive")
        .flag("x", "extract", "extract files from an archive")
        .flag("t", "list", "list the contents of an archive")
        .flag("v", "verbose", "verbosely list files processed")
        .option("f", "file", "ARCHIVE", "use archive file")
        .option("C", "directory", "DIR", "change to directory")
        .flag("", "help", "show this help")
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

    const bool create = parsed.has("create");
    const bool extract = parsed.has("extract");
    const bool list = parsed.has("list");
    if (static_cast<int>(create) + static_cast<int>(extract) + static_cast<int>(list) != 1) {
        utils::output::writeln_err("tar: must specify one of -c, -x, -t");
        return 1;
    }
    if (!parsed.has("file")) {
        utils::output::writeln_err("tar: archive file required (-f)");
        return 1;
    }
    if (parsed.has("directory")) {
        std::error_code ec;
        fs::current_path(utils::sys::path_from_utf8(parsed.get("directory")), ec);
        if (ec) {
            utils::output::writeln_err("tar: cannot change directory");
            return 1;
        }
    }

    const auto archive = utils::sys::path_from_utf8(parsed.get("file"));
    const bool verbose = parsed.has("verbose");

    if (create) {
        auto files = utils::fsutil::expand_globs(parsed.positionals);
        if (files.empty()) {
            utils::output::writeln_err("tar: no files");
            return 1;
        }
        std::ofstream out(archive, std::ios::binary);
        if (!out) {
            utils::output::writeln_err("tar: cannot write archive");
            return 1;
        }
        for (const auto& file : files) {
            if (!add_file(out, utils::sys::path_from_utf8(file), file, verbose)) {
                return 1;
            }
        }
        std::string z(1024, '\0');
        out.write(z.data(), 1024);
        return 0;
    }

    std::string data;
    std::string err;
    if (!utils::sys::read_file_bytes(archive, data, err)) {
        utils::output::writeln_err("tar: " + err);
        return 1;
    }
    for (std::size_t off = 0; off + 512 <= data.size();) {
        Ustar h{};
        std::memcpy(&h, data.data() + off, 512);
        off += 512;
        if (h.name[0] == '\0') {
            break;
        }
        const std::string name(h.name, strnlen(h.name, 100));
        const auto size = get_octal(h.size, 12);
        const std::size_t blocks = static_cast<std::size_t>((size + 511) / 512) * 512;
        if (list || verbose) {
            utils::output::writeln(name);
        }
        if (extract) {
            const auto path = utils::sys::path_from_utf8(name);
            if (h.typeflag == '5') {
                fs::create_directories(path);
            } else {
                const auto parent = path.parent_path();
                if (!parent.empty()) {
                    fs::create_directories(parent);
                }
                std::ofstream out(path, std::ios::binary);
                if (off + size <= data.size()) {
                    out.write(data.data() + off, static_cast<std::streamsize>(size));
                }
            }
        }
        off += blocks;
    }
    return 0;
}
