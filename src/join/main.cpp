#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <map>
#include <set>

namespace {

std::vector<std::string> split_fields(const std::string& line, char delim) {
    std::vector<std::string> fields;
    if (delim == 0) {
        std::size_t i = 0;
        while (i < line.size()) {
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
                ++i;
            }
            if (i >= line.size()) {
                break;
            }
            const std::size_t s = i;
            while (i < line.size() && line[i] != ' ' && line[i] != '\t') {
                ++i;
            }
            fields.push_back(line.substr(s, i - s));
        }
        return fields;
    }
    std::string cur;
    for (char c : line) {
        if (c == delim) {
            fields.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    fields.push_back(cur);
    return fields;
}

std::string field_at(const std::vector<std::string>& f, int index) {
    if (index < 1 || index > static_cast<int>(f.size())) {
        return {};
    }
    return f[static_cast<std::size_t>(index - 1)];
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("join", "Join lines of two files on a common field");
    parser.option("1", "file1-field", "FIELD", "join on this FIELD of file 1")
        .option("2", "file2-field", "FIELD", "join on this FIELD of file 2")
        .option("t", "separator", "CHAR", "use CHAR as field separator")
        .option("a", "unpairable", "FILENUM", "also print unpairable lines from file FILENUM")
        .option("v", "unpairable-only", "FILENUM", "print only unpairable lines from file FILENUM")
        .flag("", "help", "show this help")
        .positional("FILE", "two files", true);

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
    if (files.size() != 2) {
        utils::output::writeln_err("join: two files required");
        return 1;
    }

    const int f1 = parsed.has("file1-field") ? parsed.get_int("file1-field", 1) : 1;
    const int f2 = parsed.has("file2-field") ? parsed.get_int("file2-field", 1) : 1;
    char sep = 0;
    if (parsed.has("separator")) {
        const auto s = parsed.get("separator");
        if (!s.empty()) {
            sep = s[0];
        }
    }
    const char out_sep = sep ? sep : ' ';
    const int unpair_file = parsed.has("unpairable") ? parsed.get_int("unpairable", 0) : 0;
    const int only_unpair_file =
        parsed.has("unpairable-only") ? parsed.get_int("unpairable-only", 0) : 0;
    const bool unpair1 = unpair_file == 1 || only_unpair_file == 1;
    const bool unpair2 = unpair_file == 2 || only_unpair_file == 2;
    const bool only_unpair = only_unpair_file != 0;

    std::vector<std::string> a;
    std::vector<std::string> b;
    std::string err;
    if (!utils::sys::read_file_lines(utils::sys::path_from_utf8(files[0]), a, err) ||
        !utils::sys::read_file_lines(utils::sys::path_from_utf8(files[1]), b, err)) {
        utils::output::writeln_err("join: " + err);
        return 1;
    }

    std::multimap<std::string, std::vector<std::string>> right;
    for (const auto& line : b) {
        auto fields = split_fields(line, sep);
        right.emplace(field_at(fields, f2), std::move(fields));
    }
    std::set<std::string> used;
    for (const auto& line : a) {
        auto fields = split_fields(line, sep);
        const std::string key = field_at(fields, f1);
        const auto range = right.equal_range(key);
        if (range.first == range.second) {
            if (unpair1) {
                utils::output::writeln(line);
            }
            continue;
        }
        used.insert(key);
        if (only_unpair) {
            continue;
        }
        for (auto it = range.first; it != range.second; ++it) {
            std::string out = key;
            for (int i = 1; i <= static_cast<int>(fields.size()); ++i) {
                if (i == f1) {
                    continue;
                }
                out.push_back(out_sep);
                out += field_at(fields, i);
            }
            for (int i = 1; i <= static_cast<int>(it->second.size()); ++i) {
                if (i == f2) {
                    continue;
                }
                out.push_back(out_sep);
                out += field_at(it->second, i);
            }
            utils::output::writeln(out);
        }
    }
    if (unpair2) {
        for (const auto& line : b) {
            auto fields = split_fields(line, sep);
            if (used.find(field_at(fields, f2)) == used.end()) {
                utils::output::writeln(line);
            }
        }
    }
    return 0;
}
