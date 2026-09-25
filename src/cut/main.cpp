#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <cstdlib>

namespace {

struct Range {
    int start = 1;
    int end = -1;
};

bool parse_list(const std::string& text, std::vector<Range>& out, std::string& error) {
    std::string cur;
    auto add = [&](const std::string& spec) {
        if (spec.empty()) {
            return false;
        }
        Range r;
        const auto dash = spec.find('-');
        if (dash == std::string::npos) {
            r.start = r.end = std::atoi(spec.c_str());
        } else if (dash == 0) {
            r.start = 1;
            r.end = std::atoi(spec.c_str() + 1);
        } else if (dash + 1 == spec.size()) {
            r.start = std::atoi(spec.c_str());
            r.end = -1;
        } else {
            r.start = std::atoi(spec.substr(0, dash).c_str());
            r.end = std::atoi(spec.substr(dash + 1).c_str());
        }
        if (r.start < 1) {
            return false;
        }
        out.push_back(r);
        return true;
    };
    for (char c : text) {
        if (c == ',') {
            if (!add(cur)) {
                error = "cut: invalid list";
                return false;
            }
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    if (!add(cur)) {
        error = "cut: invalid list";
        return false;
    }
    return true;
}

bool wanted(int index, const std::vector<Range>& ranges) {
    for (const auto& r : ranges) {
        if (index >= r.start && (r.end < 0 || index <= r.end)) {
            return true;
        }
    }
    return false;
}

void cut_bytes(const std::string& line, const std::vector<Range>& ranges) {
    std::string out;
    for (int i = 1; i <= static_cast<int>(line.size()); ++i) {
        if (wanted(i, ranges)) {
            out.push_back(line[static_cast<std::size_t>(i - 1)]);
        }
    }
    utils::output::writeln(out);
}

void cut_fields(const std::string& line, char delim, const std::vector<Range>& ranges,
                bool only_delimited) {
    std::vector<std::string> fields;
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
    if (only_delimited && fields.size() == 1 && line.find(delim) == std::string::npos) {
        return;
    }
    std::string out;
    bool first = true;
    for (int i = 1; i <= static_cast<int>(fields.size()); ++i) {
        if (wanted(i, ranges)) {
            if (!first) {
                out.push_back(delim);
            }
            first = false;
            out += fields[static_cast<std::size_t>(i - 1)];
        }
    }
    utils::output::writeln(out);
}

int process(const std::vector<std::string>& lines, bool bytes, char delim, bool only_delim,
            const std::vector<Range>& ranges) {
    for (const auto& line : lines) {
        if (bytes) {
            cut_bytes(line, ranges);
        } else {
            cut_fields(line, delim, ranges, only_delim);
        }
    }
    return 0;
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("cut", "Remove sections from each line");
    parser.option("b", "bytes", "LIST", "select only these bytes")
        .option("c", "characters", "LIST", "select only these characters")
        .option("f", "fields", "LIST", "select only these fields")
        .option("d", "delimiter", "DELIM", "field delimiter (default TAB)")
        .flag("s", "only-delimited", "do not print lines not containing delimiters")
        .flag("", "help", "show this help")
        .positional("FILE", "file to cut", true);

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

    const bool use_bytes = parsed.has("bytes") || parsed.has("characters");
    const bool use_fields = parsed.has("fields");
    if (use_bytes == use_fields) {
        utils::output::writeln_err("cut: you must specify a list of bytes, characters, or fields");
        return 1;
    }
    std::vector<Range> ranges;
    std::string err;
    if (!parse_list(use_bytes ? (parsed.has("bytes") ? parsed.get("bytes") : parsed.get("characters"))
                              : parsed.get("fields"),
                    ranges, err)) {
        utils::output::writeln_err(err);
        return 1;
    }
    char delim = '\t';
    if (parsed.has("delimiter")) {
        const auto d = parsed.get("delimiter");
        if (d.empty()) {
            utils::output::writeln_err("cut: empty delimiter");
            return 1;
        }
        delim = d[0];
    }

    auto files = utils::fsutil::expand_globs(parsed.positionals);
    if (files.empty()) {
        return process(utils::sys::read_stdin_lines(), use_bytes, delim, parsed.has("only-delimited"),
                       ranges);
    }
    bool had_error = false;
    for (const auto& file : files) {
        std::vector<std::string> lines;
        std::string e;
        if (!utils::sys::read_file_lines(utils::sys::path_from_utf8(file), lines, e)) {
            utils::output::writeln_err("cut: " + file + ": " + e);
            had_error = true;
            continue;
        }
        process(lines, use_bytes, delim, parsed.has("only-delimited"), ranges);
    }
    return had_error ? 1 : 0;
}
