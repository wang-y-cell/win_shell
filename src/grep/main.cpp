#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/sys.h"
#include "utils/theme.h"

#include <cctype>
#include <filesystem>
#include <regex>
#include <sstream>
#include <system_error>

namespace {

struct Options {
    bool ignore_case = false;
    bool invert = false;
    bool line_number = false;
    bool fixed = false;
    bool count_only = false;
    bool word = false;
    bool files_with = false;
    bool only_matching = false;
    bool with_filename = false;
    bool quiet = false;
    bool silent = false;
    bool whole_line = false;
    bool extended = false;
    bool recursive = false;
    std::string pattern_file;
    std::vector<std::string> extra_patterns;
};

std::string regex_escape(const std::string& text) {
    static const std::string special = R"(\.^$|()[]{}*+?)";
    std::string out;
    for (char c : text) {
        if (special.find(c) != std::string::npos) {
            out.push_back('\\');
        }
        out.push_back(c);
    }
    return out;
}

bool literal_match(const std::string& line, const std::string& pat, bool icase) {
    if (pat.empty()) {
        return true;
    }
    if (!icase) {
        return line.find(pat) != std::string::npos;
    }
    auto fold = [](unsigned char c) { return static_cast<char>(std::tolower(c)); };
    if (pat.size() > line.size()) {
        return false;
    }
    for (std::size_t i = 0; i + pat.size() <= line.size(); ++i) {
        bool ok = true;
        for (std::size_t j = 0; j < pat.size(); ++j) {
            if (fold(static_cast<unsigned char>(line[i + j])) !=
                fold(static_cast<unsigned char>(pat[j]))) {
                ok = false;
                break;
            }
        }
        if (ok) {
            return true;
        }
    }
    return false;
}

void print_match_line(const std::string& prefix, const std::string& line, const std::regex* re,
                      bool colorize) {
    if (!colorize || re == nullptr) {
        utils::output::writeln(prefix + line);
        return;
    }
    utils::output::write(prefix);
    std::sregex_iterator it(line.begin(), line.end(), *re);
    std::sregex_iterator end;
    std::size_t last = 0;
    for (; it != end; ++it) {
        const auto pos = static_cast<std::size_t>(it->position());
        const auto len = static_cast<std::size_t>(it->length());
        if (pos > last) {
            utils::output::write(line.substr(last, pos - last));
        }
        utils::output::write(line.substr(pos, len), utils::theme::red());
        last = pos + len;
    }
    if (last < line.size()) {
        utils::output::write(line.substr(last));
    }
    utils::output::writeln("");
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    const auto args = utils::sys::utf8_argv(argc, argv);
    Options opt;
    std::vector<std::string> rest;
    for (std::size_t i = 1; i < args.size(); ++i) {
        const auto& tok = args[i];
        if (tok == "--help") {
            utils::output::writeln("Usage: grep [OPTION]... PATTERN [FILE]...");
            utils::output::writeln("  -i  ignore case");
            utils::output::writeln("  -v  invert match");
            utils::output::writeln("  -n  line numbers");
            utils::output::writeln("  -F  fixed strings");
            utils::output::writeln("  -E  extended regular expressions");
            utils::output::writeln("  -e  PATTERN");
            utils::output::writeln("  -f  FILE of patterns");
            utils::output::writeln("  -c  count matches");
            utils::output::writeln("  -w  word match");
            utils::output::writeln("  -x  whole line");
            utils::output::writeln("  -l  files with matches");
            utils::output::writeln("  -o  only matching");
            utils::output::writeln("  -H  print filename");
            utils::output::writeln("  -q  quiet, only exit status");
            utils::output::writeln("  -s  suppress error messages");
            utils::output::writeln("  -r  recursive");
            return 0;
        }
        if (tok == "-f" || tok == "-e") {
            if (i + 1 >= args.size()) {
                utils::output::writeln_err(std::string("grep: option requires an argument -- ") +
                                           tok.back());
                return 2;
            }
            if (tok == "-f") {
                opt.pattern_file = args[++i];
            } else {
                opt.extra_patterns.push_back(args[++i]);
            }
            continue;
        }
        if (tok.size() >= 2 && tok[0] == '-' && tok[1] != '-') {
            for (std::size_t k = 1; k < tok.size(); ++k) {
                switch (tok[k]) {
                    case 'i':
                        opt.ignore_case = true;
                        break;
                    case 'v':
                        opt.invert = true;
                        break;
                    case 'n':
                        opt.line_number = true;
                        break;
                    case 'F':
                        opt.fixed = true;
                        break;
                    case 'E':
                        opt.extended = true;
                        break;
                    case 'c':
                        opt.count_only = true;
                        break;
                    case 'w':
                        opt.word = true;
                        break;
                    case 'x':
                        opt.whole_line = true;
                        break;
                    case 'l':
                        opt.files_with = true;
                        break;
                    case 'o':
                        opt.only_matching = true;
                        break;
                    case 'H':
                        opt.with_filename = true;
                        break;
                    case 'q':
                        opt.quiet = true;
                        break;
                    case 's':
                        opt.silent = true;
                        break;
                    case 'r':
                    case 'R':
                        opt.recursive = true;
                        break;
                    case 'f':
                    case 'e':
                        utils::output::writeln_err(std::string("grep: option requires an argument -- ") +
                                                   tok[k]);
                        return 2;
                    default:
                        utils::output::writeln_err(std::string("grep: invalid option -- '") + tok[k] +
                                                   "'");
                        return 2;
                }
            }
            continue;
        }
        rest.push_back(tok);
    }

    std::vector<std::string> patterns = opt.extra_patterns;
    if (!opt.pattern_file.empty()) {
        const auto path = utils::sys::path_from_utf8(opt.pattern_file);
        std::string err;
        if (!utils::sys::read_file_lines(path, patterns, err)) {
            if (!opt.silent) {
                utils::output::writeln_err("grep: " + opt.pattern_file + ": " + err);
            }
            return 2;
        }
    }
    std::vector<std::string> files;
    if (patterns.empty()) {
        if (rest.empty()) {
            utils::output::writeln_err("grep: missing pattern");
            return 2;
        }
        patterns.push_back(rest[0]);
        files.assign(rest.begin() + 1, rest.end());
    } else {
        files = rest;
    }
    files = utils::fsutil::expand_globs(files);
    if (opt.recursive) {
        std::vector<std::string> expanded;
        for (const auto& file : files) {
            const auto path = utils::sys::path_from_utf8(file);
            if (utils::fsutil::is_dir(path)) {
                std::error_code ec;
                for (auto it = std::filesystem::recursive_directory_iterator(
                         path, std::filesystem::directory_options::skip_permission_denied, ec);
                     it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
                    if (ec) {
                        ec.clear();
                        continue;
                    }
                    if (it->is_regular_file(ec)) {
                        expanded.push_back(utils::sys::path_to_utf8(it->path()));
                    }
                }
            } else {
                expanded.push_back(file);
            }
        }
        files = std::move(expanded);
        opt.with_filename = true;
    }

    std::string body;
    if (opt.extended) {
        opt.fixed = false;
    }
    if (opt.fixed) {
        for (std::size_t i = 0; i < patterns.size(); ++i) {
            if (i) {
                body += "|";
            }
            body += regex_escape(patterns[i]);
        }
    } else {
        for (std::size_t i = 0; i < patterns.size(); ++i) {
            if (i) {
                body += "|";
            }
            body += "(?:" + patterns[i] + ")";
        }
    }
    if (opt.word) {
        body = "\\b(?:" + body + ")\\b";
    }
    if (opt.whole_line) {
        body = "^(?:" + body + ")$";
    }

    std::regex re;
    const auto flags =
        opt.ignore_case ? (std::regex::ECMAScript | std::regex::icase) : std::regex::ECMAScript;
    try {
        re = std::regex(body, flags);
    } catch (const std::regex_error& e) {
        utils::output::writeln_err(std::string("grep: invalid pattern: ") + e.what());
        return 2;
    }

    const bool colorize = utils::output::color_enabled() && !opt.count_only && !opt.files_with;
    const bool multi = files.size() > 1 || opt.with_filename;
    bool any_match = false;
    bool had_error = false;

    auto process = [&](const std::string& label, const std::vector<std::string>& lines) {
        int matches = 0;
        for (std::size_t i = 0; i < lines.size(); ++i) {
            const auto& line = lines[i];
            bool matched = opt.fixed && patterns.size() == 1 && !opt.word && !opt.whole_line
                               ? literal_match(line, patterns[0], opt.ignore_case)
                               : (opt.whole_line ? std::regex_match(line, re)
                                                 : std::regex_search(line, re));
            if (opt.invert) {
                matched = !matched;
            }
            if (!matched) {
                continue;
            }
            ++matches;
            any_match = true;
            if (opt.quiet) {
                continue;
            }
            if (opt.files_with) {
                utils::output::writeln(label.empty() ? "(standard input)" : label);
                return;
            }
            if (opt.count_only) {
                continue;
            }
            std::string prefix;
            if (multi && !label.empty()) {
                prefix += label + ":";
            }
            if (opt.line_number) {
                prefix += std::to_string(i + 1) + ":";
            }
            if (opt.only_matching && !opt.invert) {
                for (std::sregex_iterator it(line.begin(), line.end(), re), end; it != end; ++it) {
                    if (colorize) {
                        utils::output::writeln(prefix + it->str(), utils::theme::red());
                    } else {
                        utils::output::writeln(prefix + it->str());
                    }
                }
            } else {
                print_match_line(prefix, line, opt.invert ? nullptr : &re, colorize);
            }
        }
        if (opt.count_only && !opt.quiet) {
            std::string prefix;
            if (multi && !label.empty()) {
                prefix = label + ":";
            }
            utils::output::writeln(prefix + std::to_string(matches));
        }
    };

    if (files.empty()) {
        process("", utils::sys::read_stdin_lines());
    } else {
        for (const auto& file : files) {
            const auto path = utils::sys::path_from_utf8(file);
            if (!utils::fsutil::exists(path)) {
                if (!opt.silent) {
                    utils::output::writeln_err("grep: " + file + ": No such file or directory");
                }
                had_error = true;
                continue;
            }
            if (utils::fsutil::is_dir(path)) {
                if (!opt.silent) {
                    utils::output::writeln_err("grep: " + file + ": Is a directory");
                }
                had_error = true;
                continue;
            }
            std::vector<std::string> lines;
            std::string err;
            if (!utils::sys::read_file_lines(path, lines, err)) {
                if (!opt.silent) {
                    utils::output::writeln_err("grep: " + file + ": " + err);
                }
                had_error = true;
                continue;
            }
            process(file, lines);
        }
    }

    if (opt.quiet && any_match) {
        return 0;
    }
    if (had_error) {
        return 2;
    }
    return any_match ? 0 : 1;
}
