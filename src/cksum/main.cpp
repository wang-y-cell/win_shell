#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <cstdint>

namespace {

std::uint32_t posix_cksum(const std::string& data) {
    std::uint32_t crc = 0;
    for (unsigned char c : data) {
        crc = (crc << 8) ^ c;
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x80000000u) {
                crc = (crc << 1) ^ 0x04C11DB7u;
            } else {
                crc <<= 1;
            }
        }
    }
    std::uint64_t len = data.size();
    while (len != 0) {
        crc = (crc << 8) ^ static_cast<std::uint32_t>(len & 0xFFu);
        for (int i = 0; i < 8; ++i) {
            if (crc & 0x80000000u) {
                crc = (crc << 1) ^ 0x04C11DB7u;
            } else {
                crc <<= 1;
            }
        }
        len >>= 8;
    }
    return ~crc;
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("cksum", "Print CRC checksum and byte counts");
    parser.flag("", "help", "show this help").positional("FILE", "file to checksum", true);

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
        std::string data;
        for (const auto& line : utils::sys::read_stdin_lines()) {
            data += line;
            data += '\n';
        }
        utils::output::writeln(std::to_string(posix_cksum(data)) + " " + std::to_string(data.size()));
        return 0;
    }
    bool had_error = false;
    for (const auto& file : files) {
        std::string data;
        std::string err;
        if (!utils::sys::read_file_bytes(utils::sys::path_from_utf8(file), data, err)) {
            utils::output::writeln_err("cksum: " + file + ": " + err);
            had_error = true;
            continue;
        }
        utils::output::writeln(std::to_string(posix_cksum(data)) + " " + std::to_string(data.size()) +
                               " " + file);
    }
    return had_error ? 1 : 0;
}
