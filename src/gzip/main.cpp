#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#endif

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

std::string gzip_wrap(const std::string& data) {
    std::string out;
    const unsigned char hdr[] = {0x1f, 0x8b, 0x08, 0x00, 0, 0, 0, 0, 0x00, 0xff};
    out.append(reinterpret_cast<const char*>(hdr), sizeof(hdr));
    std::size_t i = 0;
    while (i < data.size()) {
        const std::size_t n = std::min<std::size_t>(65535, data.size() - i);
        const bool last = i + n >= data.size();
        out.push_back(static_cast<char>(last ? 1 : 0));
        const auto len = static_cast<std::uint16_t>(n);
        const auto nlen = static_cast<std::uint16_t>(~len);
        out.push_back(static_cast<char>(len & 0xFF));
        out.push_back(static_cast<char>((len >> 8) & 0xFF));
        out.push_back(static_cast<char>(nlen & 0xFF));
        out.push_back(static_cast<char>((nlen >> 8) & 0xFF));
        out.append(data.data() + i, n);
        i += n;
    }
    const auto crc = crc32_of(data);
    const auto sz = static_cast<std::uint32_t>(data.size());
    for (int s = 0; s < 32; s += 8) {
        out.push_back(static_cast<char>((crc >> s) & 0xFF));
    }
    for (int s = 0; s < 32; s += 8) {
        out.push_back(static_cast<char>((sz >> s) & 0xFF));
    }
    return out;
}

bool gzip_unwrap(const std::string& in, std::string& out, std::string& error) {
    if (in.size() < 18 || static_cast<unsigned char>(in[0]) != 0x1f ||
        static_cast<unsigned char>(in[1]) != 0x8b || in[2] != 8) {
        error = "not in gzip format";
        return false;
    }
    const auto flags = static_cast<unsigned char>(in[3]);
    std::size_t i = 10;
    if (flags & 4) {
        if (i + 2 > in.size()) {
            error = "truncated";
            return false;
        }
        i += 2 + static_cast<unsigned char>(in[i]) + (static_cast<unsigned>(in[i + 1]) << 8);
    }
    if (flags & 8) {
        while (i < in.size() && in[i]) {
            ++i;
        }
        ++i;
    }
    if (flags & 16) {
        while (i < in.size() && in[i]) {
            ++i;
        }
        ++i;
    }
    if (flags & 2) {
        i += 2;
    }
    out.clear();
    while (i + 5 <= in.size() - 8) {
        const auto bfinal = static_cast<unsigned char>(in[i]) & 1;
        const auto btype = (static_cast<unsigned char>(in[i]) >> 1) & 3;
        ++i;
        if (btype != 0) {
            error = "compressed gzip blocks are not supported (store-only)";
            return false;
        }
        if (i + 4 > in.size()) {
            error = "truncated";
            return false;
        }
        const auto len = static_cast<unsigned char>(in[i]) |
                         (static_cast<unsigned>(static_cast<unsigned char>(in[i + 1])) << 8);
        i += 4;
        if (i + len > in.size()) {
            error = "truncated";
            return false;
        }
        out.append(in.data() + i, len);
        i += len;
        if (bfinal) {
            break;
        }
    }
    return true;
}

std::string out_name_compress(std::string name) {
    return name + ".gz";
}

std::string out_name_decompress(std::string name) {
    if (name.size() > 3 && name.substr(name.size() - 3) == ".gz") {
        return name.substr(0, name.size() - 3);
    }
    return name + ".out";
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
#ifdef GZIP_AS_GUNZIP
    utils::Parser parser("gunzip", "Decompress gzip files");
    const bool default_decompress = true;
#else
    utils::Parser parser("gzip", "Compress or decompress files");
    const bool default_decompress = false;
#endif
    parser.flag("d", "decompress", "decompress")
        .flag("c", "stdout", "write on standard output")
        .flag("k", "keep", "keep original files")
        .flag("f", "force", "force overwrite")
        .flag("", "help", "show this help")
        .positional("FILE", "file to process", true);

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

    const bool decompress = default_decompress || parsed.has("decompress");
    const bool to_stdout = parsed.has("stdout");
    const bool keep = parsed.has("keep");
#ifdef _WIN32
    _setmode(_fileno(stdin), _O_BINARY);
    _setmode(_fileno(stdout), _O_BINARY);
#endif

    auto files = utils::fsutil::expand_globs(parsed.positionals);
    if (files.empty()) {
        std::string data;
        char buf[4096];
        while (std::cin.read(buf, sizeof(buf)) || std::cin.gcount() > 0) {
            data.append(buf, static_cast<std::size_t>(std::cin.gcount()));
            if (!std::cin) {
                break;
            }
        }
        if (decompress) {
            std::string out;
            std::string err;
            if (!gzip_unwrap(data, out, err)) {
                utils::output::writeln_err("gzip: " + err);
                return 1;
            }
            utils::output::write(out);
        } else {
            utils::output::write(gzip_wrap(data));
        }
        return 0;
    }

    bool had_error = false;
    for (const auto& file : files) {
        std::string data;
        std::string err;
        if (!utils::sys::read_file_bytes(utils::sys::path_from_utf8(file), data, err)) {
            utils::output::writeln_err("gzip: " + file + ": " + err);
            had_error = true;
            continue;
        }
        std::string out;
        if (decompress) {
            if (!gzip_unwrap(data, out, err)) {
                utils::output::writeln_err("gzip: " + file + ": " + err);
                had_error = true;
                continue;
            }
        } else {
            out = gzip_wrap(data);
        }
        if (to_stdout) {
            utils::output::write(out);
            continue;
        }
        const auto dest = decompress ? out_name_decompress(file) : out_name_compress(file);
        const auto path = utils::sys::path_from_utf8(dest);
        if (utils::fsutil::exists(path) && !parsed.has("force")) {
            utils::output::writeln_err("gzip: " + dest + " already exists");
            had_error = true;
            continue;
        }
        std::ofstream ofs(path, std::ios::binary);
        ofs.write(out.data(), static_cast<std::streamsize>(out.size()));
        if (!keep) {
            std::filesystem::remove(utils::sys::path_from_utf8(file));
        }
    }
    return had_error ? 1 : 0;
}
