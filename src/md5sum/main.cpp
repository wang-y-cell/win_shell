#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <filesystem>
#include <fstream>
#include <iomanip>
#include <sstream>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <wincrypt.h>
#endif

namespace {

#ifdef _WIN32
bool hash_file(const std::filesystem::path& path, std::string& hex, std::string& error) {
    std::string data;
    if (!utils::sys::read_file_bytes(path, data, error)) {
        return false;
    }
    HCRYPTPROV prov = 0;
    HCRYPTHASH hash = 0;
    if (!CryptAcquireContextW(&prov, nullptr, nullptr, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT)) {
        error = "crypto init failed";
        return false;
    }
    if (!CryptCreateHash(prov, CALG_MD5, 0, 0, &hash)) {
        CryptReleaseContext(prov, 0);
        error = "hash init failed";
        return false;
    }
    if (!data.empty() &&
        !CryptHashData(hash, reinterpret_cast<const BYTE*>(data.data()),
                       static_cast<DWORD>(data.size()), 0)) {
        CryptDestroyHash(hash);
        CryptReleaseContext(prov, 0);
        error = "hash failed";
        return false;
    }
    BYTE buf[16];
    DWORD len = 16;
    CryptGetHashParam(hash, HP_HASHVAL, buf, &len, 0);
    CryptDestroyHash(hash);
    CryptReleaseContext(prov, 0);
    std::ostringstream out;
    out << std::hex << std::setfill('0');
    for (DWORD i = 0; i < len; ++i) {
        out << std::setw(2) << static_cast<int>(buf[i]);
    }
    hex = out.str();
    return true;
}
#endif

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("md5sum", "Compute and check MD5 message digest");
    parser.flag("c", "check", "read checksums from the FILEs and check them")
        .flag("", "help", "show this help")
        .positional("FILE", "file to hash, or checksum list with -c", true);

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
    bool had_error = false;
    if (parsed.has("check")) {
        if (files.empty()) {
            utils::output::writeln_err("md5sum: missing operand");
            return 1;
        }
        for (const auto& list : files) {
            std::vector<std::string> lines;
            std::string err;
            if (!utils::sys::read_file_lines(utils::sys::path_from_utf8(list), lines, err)) {
                utils::output::writeln_err("md5sum: " + list + ": " + err);
                had_error = true;
                continue;
            }
            for (const auto& line : lines) {
                if (line.size() < 34) {
                    continue;
                }
                const std::string expect = line.substr(0, 32);
                std::size_t i = 32;
                while (i < line.size() && (line[i] == ' ' || line[i] == '*')) {
                    ++i;
                }
                const std::string name = line.substr(i);
                std::string hex;
                if (!hash_file(utils::sys::path_from_utf8(name), hex, err)) {
                    utils::output::writeln(name + ": FAILED");
                    had_error = true;
                    continue;
                }
                if (hex == expect) {
                    utils::output::writeln(name + ": OK");
                } else {
                    utils::output::writeln(name + ": FAILED");
                    had_error = true;
                }
            }
        }
        return had_error ? 1 : 0;
    }

    if (files.empty()) {
        files.emplace_back("-");
    }
    for (const auto& file : files) {
        std::string hex;
        std::string err;
        if (file == "-") {
            std::string data;
            for (const auto& line : utils::sys::read_stdin_lines()) {
                data += line;
                data += '\n';
            }
#ifdef _WIN32
            const auto tmp = std::filesystem::temp_directory_path() / "md5sum-stdin";
            {
                std::ofstream out(tmp, std::ios::binary);
                out << data;
            }
            if (!hash_file(tmp, hex, err)) {
                utils::output::writeln_err("md5sum: " + err);
                return 1;
            }
            std::filesystem::remove(tmp);
#endif
            utils::output::writeln(hex + "  -");
            continue;
        }
        if (!hash_file(utils::sys::path_from_utf8(file), hex, err)) {
            utils::output::writeln_err("md5sum: " + file + ": " + err);
            had_error = true;
            continue;
        }
        utils::output::writeln(hex + "  " + file);
    }
    return had_error ? 1 : 0;
}
