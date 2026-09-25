#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <string>

namespace {

std::string expand_set(const std::string& spec) {
    std::string out;
    for (std::size_t i = 0; i < spec.size(); ++i) {
        if (i + 2 < spec.size() && spec[i + 1] == '-' && spec[i] <= spec[i + 2]) {
            for (char c = spec[i]; c <= spec[i + 2]; ++c) {
                out.push_back(c);
            }
            i += 2;
        } else {
            out.push_back(spec[i]);
        }
    }
    return out;
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("tr", "Translate or delete characters");
    parser.flag("d", "delete", "delete characters in SET1")
        .flag("s", "squeeze-repeats", "replace each sequence of a repeated character")
        .flag("c", "complement", "use the complement of SET1")
        .flag("", "help", "show this help")
        .positional("SET", "SET1 and optional SET2", true);

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
        utils::output::writeln_err("tr: missing operand");
        return 1;
    }

    std::string set1 = expand_set(parsed.positionals[0]);
    std::string set2 = parsed.positionals.size() > 1 ? expand_set(parsed.positionals[1]) : "";
    const bool del = parsed.has("delete");
    const bool squeeze = parsed.has("squeeze-repeats");
    const bool complement = parsed.has("complement");

    bool in_set1[256] = {};
    for (unsigned char c : set1) {
        in_set1[c] = true;
    }
    if (complement) {
        std::string comp;
        for (int i = 0; i < 256; ++i) {
            if (!in_set1[i]) {
                comp.push_back(static_cast<char>(i));
            }
        }
        set1 = std::move(comp);
        for (bool& b : in_set1) {
            b = false;
        }
        for (unsigned char c : set1) {
            in_set1[c] = true;
        }
    }

    char map[256];
    for (int i = 0; i < 256; ++i) {
        map[i] = static_cast<char>(i);
    }
    if (!del && !set2.empty()) {
        for (std::size_t i = 0; i < set1.size(); ++i) {
            const char to = i < set2.size() ? set2[i] : set2.back();
            map[static_cast<unsigned char>(set1[i])] = to;
        }
    }

    bool squeeze_set[256] = {};
    if (squeeze) {
        const std::string& src = set2.empty() ? set1 : set2;
        for (unsigned char c : src) {
            squeeze_set[c] = true;
        }
    }

    int prev = -1;
    for (const auto& line : utils::sys::read_stdin_lines()) {
        std::string out;
        for (unsigned char c : line) {
            if (del && in_set1[c]) {
                continue;
            }
            const char mapped = del ? static_cast<char>(c) : map[c];
            if (squeeze && squeeze_set[static_cast<unsigned char>(mapped)] &&
                prev == static_cast<unsigned char>(mapped)) {
                continue;
            }
            out.push_back(mapped);
            prev = static_cast<unsigned char>(mapped);
        }
        prev = -1;
        utils::output::writeln(out);
    }
    return 0;
}
