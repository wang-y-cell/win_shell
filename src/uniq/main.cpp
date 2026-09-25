#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <algorithm>
#include <cctype>
#include <iomanip>
#include <sstream>

namespace {

std::string compare_key(const std::string& line, int skip_fields, int skip_chars) {
    std::size_t i = 0;
    for (int f = 0; f < skip_fields && i < line.size(); ++f) {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) {
            ++i;
        }
        while (i < line.size() && line[i] != ' ' && line[i] != '\t') {
            ++i;
        }
    }
    i = std::min(i + static_cast<std::size_t>(std::max(0, skip_chars)), line.size());
    return line.substr(i);
}

bool same_line(const std::string& a, const std::string& b, bool ignore_case) {
    if (!ignore_case) {
        return a == b;
    }
    if (a.size() != b.size()) {
        return false;
    }
    for (std::size_t i = 0; i < a.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(a[i])) !=
            std::tolower(static_cast<unsigned char>(b[i]))) {
            return false;
        }
    }
    return true;
}

void emit_run(const std::string& line, int count, bool show_count, bool only_dup, bool only_unique) {
    if (only_dup && count < 2) {
        return;
    }
    if (only_unique && count != 1) {
        return;
    }
    if (show_count) {
        std::ostringstream out;
        out << std::setw(7) << count << ' ' << line;
        utils::output::writeln(out.str());
    } else {
        utils::output::writeln(line);
    }
}

void uniq_lines(const std::vector<std::string>& lines, bool show_count, bool ignore_case,
                bool only_dup, bool only_unique, int skip_fields, int skip_chars) {
    if (lines.empty()) {
        return;
    }
    std::string prev = lines[0];
    std::string prev_key = compare_key(prev, skip_fields, skip_chars);
    int count = 1;
    for (std::size_t i = 1; i < lines.size(); ++i) {
        const std::string key = compare_key(lines[i], skip_fields, skip_chars);
        if (same_line(key, prev_key, ignore_case)) {
            ++count;
            continue;
        }
        emit_run(prev, count, show_count, only_dup, only_unique);
        prev = lines[i];
        prev_key = key;
        count = 1;
    }
    emit_run(prev, count, show_count, only_dup, only_unique);
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("uniq", "Report or omit repeated lines");
    parser.flag("c", "count", "prefix lines by the number of occurrences")
        .flag("i", "ignore-case", "ignore differences in case")
        .flag("d", "repeated", "only print duplicate lines")
        .flag("u", "unique", "only print unique lines")
        .option("f", "skip-fields", "N", "avoid comparing the first N fields")
        .option("s", "skip-chars", "N", "avoid comparing the first N characters")
        .flag("", "help", "show this help")
        .positional("FILE", "file to read", true);

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

    const bool show_count = parsed.has("count");
    const bool ignore_case = parsed.has("ignore-case");
    const bool only_dup = parsed.has("repeated");
    const bool only_unique = parsed.has("unique");
    const int skip_fields = parsed.get_int("skip-fields", 0);
    const int skip_chars = parsed.get_int("skip-chars", 0);
    auto files = utils::fsutil::expand_globs(parsed.positionals);

    if (files.empty()) {
        uniq_lines(utils::sys::read_stdin_lines(), show_count, ignore_case, only_dup, only_unique,
                   skip_fields, skip_chars);
        return 0;
    }

    bool had_error = false;
    for (const auto& file : files) {
        const auto path = utils::sys::path_from_utf8(file);
        if (!utils::fsutil::exists(path)) {
            utils::output::writeln_err("uniq: " + file + ": No such file or directory");
            had_error = true;
            continue;
        }
        if (utils::fsutil::is_dir(path)) {
            utils::output::writeln_err("uniq: " + file + ": Is a directory");
            had_error = true;
            continue;
        }
        std::vector<std::string> lines;
        std::string err;
        if (!utils::sys::read_file_lines(path, lines, err)) {
            utils::output::writeln_err("uniq: " + file + ": " + err);
            had_error = true;
            continue;
        }
        uniq_lines(lines, show_count, ignore_case, only_dup, only_unique, skip_fields, skip_chars);
    }
    return had_error ? 1 : 0;
}
