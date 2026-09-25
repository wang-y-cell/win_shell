#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>

namespace {

std::string to_lower(std::string text) {
    for (char& c : text) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return text;
}

std::string skip_blanks(const std::string& text) {
    std::size_t i = 0;
    while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) {
        ++i;
    }
    return text.substr(i);
}

std::vector<std::string> split_fields(const std::string& line, char sep) {
    std::vector<std::string> fields;
    if (sep == 0) {
        std::size_t i = 0;
        while (i < line.size()) {
            while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
                ++i;
            }
            if (i >= line.size()) {
                break;
            }
            const std::size_t start = i;
            while (i < line.size() && line[i] != ' ' && line[i] != '\t') {
                ++i;
            }
            fields.push_back(line.substr(start, i - start));
        }
        return fields;
    }
    std::size_t start = 0;
    for (std::size_t i = 0; i <= line.size(); ++i) {
        if (i == line.size() || line[i] == sep) {
            fields.push_back(line.substr(start, i - start));
            start = i + 1;
        }
    }
    return fields;
}

bool parse_key(const std::string& spec, int& start, int& end, std::string& error) {
    if (spec.empty()) {
        error = "sort: invalid key";
        return false;
    }
    const auto comma = spec.find(',');
    const std::string a = spec.substr(0, comma);
    const std::string b = comma == std::string::npos ? "" : spec.substr(comma + 1);
    auto field = [](const std::string& text, int& out) {
        const auto dot = text.find('.');
        const std::string body = dot == std::string::npos ? text : text.substr(0, dot);
        if (body.empty()) {
            return false;
        }
        char* p = nullptr;
        const long n = std::strtol(body.c_str(), &p, 10);
        if (p != body.c_str() + body.size() || n < 1) {
            return false;
        }
        out = static_cast<int>(n);
        return true;
    };
    if (!field(a, start)) {
        error = "sort: invalid key '" + spec + "'";
        return false;
    }
    end = start;
    if (!b.empty() && !field(b, end)) {
        error = "sort: invalid key '" + spec + "'";
        return false;
    }
    return true;
}

std::string key_of(const std::string& line, char sep, int start, int end, bool blanks, bool fold) {
    std::string key;
    if (start <= 0) {
        key = blanks ? skip_blanks(line) : line;
    } else {
        const auto fields = split_fields(line, sep);
        std::ostringstream out;
        const int from = std::max(1, start);
        const int to = std::max(from, end);
        for (int i = from; i <= to; ++i) {
            if (i > from) {
                out << (sep ? sep : ' ');
            }
            if (i <= static_cast<int>(fields.size())) {
                out << fields[static_cast<std::size_t>(i - 1)];
            }
        }
        key = out.str();
        if (blanks) {
            key = skip_blanks(key);
        }
    }
    return fold ? to_lower(key) : key;
}

double leading_number(const std::string& text) {
    std::size_t i = 0;
    while (i < text.size() && (text[i] == ' ' || text[i] == '\t')) {
        ++i;
    }
    if (i >= text.size()) {
        return NAN;
    }
    char* end = nullptr;
    const double value = std::strtod(text.c_str() + i, &end);
    if (end == text.c_str() + i) {
        return NAN;
    }
    return value;
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("sort", "Sort lines of text files");
    parser.flag("r", "reverse", "reverse the result of comparisons")
        .flag("n", "numeric-sort", "compare according to string numerical value")
        .flag("u", "unique", "output only the first of an equal run")
        .flag("f", "ignore-case", "fold lower case to upper case characters")
        .flag("b", "ignore-leading-blanks", "ignore leading blanks")
        .option("t", "field-separator", "SEP", "use SEP instead of non-blank to blank transition")
        .option("k", "key", "KEY", "sort via a key; KEY is FIELD[.CHAR][,FIELD[.CHAR]]")
        .option("o", "output", "FILE", "write result to FILE instead of standard output")
        .flag("", "help", "show this help")
        .positional("FILE", "file to sort", true);

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

    const bool reverse = parsed.has("reverse");
    const bool numeric = parsed.has("numeric-sort");
    const bool unique = parsed.has("unique");
    const bool fold = parsed.has("ignore-case");
    const bool blanks = parsed.has("ignore-leading-blanks");
    char sep = 0;
    if (parsed.has("field-separator")) {
        const auto s = parsed.get("field-separator");
        if (s.empty()) {
            utils::output::writeln_err("sort: empty field separator");
            return 1;
        }
        sep = s[0];
    }
    int key_start = 0;
    int key_end = 0;
    if (parsed.has("key")) {
        std::string err;
        if (!parse_key(parsed.get("key"), key_start, key_end, err)) {
            utils::output::writeln_err(err);
            return 1;
        }
    }
    auto files = utils::fsutil::expand_globs(parsed.positionals);

    std::vector<std::string> lines;
    if (files.empty()) {
        lines = utils::sys::read_stdin_lines();
    } else {
        for (const auto& file : files) {
            const auto path = utils::sys::path_from_utf8(file);
            if (!utils::fsutil::exists(path)) {
                utils::output::writeln_err("sort: open failed: " + file +
                                           ": No such file or directory");
                continue;
            }
            if (utils::fsutil::is_dir(path)) {
                utils::output::writeln_err("sort: read failed: " + file + ": Is a directory");
                continue;
            }
            std::string err;
            if (!utils::sys::read_file_lines(path, lines, err)) {
                utils::output::writeln_err("sort: " + file + ": " + err);
            }
        }
    }

    auto less = [&](const std::string& a, const std::string& b) {
        const std::string ka = key_of(a, sep, key_start, key_end, blanks, fold);
        const std::string kb = key_of(b, sep, key_start, key_end, blanks, fold);
        if (numeric) {
            const double na = leading_number(ka);
            const double nb = leading_number(kb);
            const bool a_nan = std::isnan(na);
            const bool b_nan = std::isnan(nb);
            if (a_nan != b_nan) {
                return a_nan;
            }
            if (!a_nan && na != nb) {
                return na < nb;
            }
        }
        return ka < kb;
    };
    std::stable_sort(lines.begin(), lines.end(), less);
    if (reverse) {
        std::reverse(lines.begin(), lines.end());
    }

    std::ofstream out_file;
    std::ostream* out = &std::cout;
    if (parsed.has("output")) {
        const auto path = utils::sys::path_from_utf8(parsed.get("output"));
        out_file.open(path, std::ios::binary | std::ios::trunc);
        if (!out_file) {
            utils::output::writeln_err("sort: cannot open '" + parsed.get("output") + "'");
            return 1;
        }
        out = &out_file;
    }

    std::string prev;
    bool has_prev = false;
    for (const auto& line : lines) {
        if (unique && has_prev) {
            const std::string ka = key_of(line, sep, key_start, key_end, blanks, fold);
            const std::string kb = key_of(prev, sep, key_start, key_end, blanks, fold);
            if (ka == kb) {
                continue;
            }
        }
        *out << line << '\n';
        prev = line;
        has_prev = true;
    }
    return 0;
}
