#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"
#include "utils/width.h"

#include <iomanip>
#include <sstream>
#include <vector>

namespace {

constexpr std::size_t kPrefixCols = 8;

struct ShowFlags {
    bool nonprinting = false;
    bool ends = false;
    bool tabs = false;
};

//判断是否是空行
bool is_blank(const std::string& line) {
    //找第一个不是空格,tab,回车的字符,如果有说明不是空行,否则就是空行
    return line.find_first_not_of(" \t\r") == std::string::npos;
}

//将控制字符映射到可见字符
void append_caret(std::string& out, unsigned char c) {
    //127是删除字符
    if (c == 127) {
        out += "^?";
        return;
    }
    //0-31是控制字符
    out += '^';
    //将控制字符映射到@A-Z[\]^_`
    out += static_cast<char>(c + 64);
}

std::string make_visible(const std::string& line, const ShowFlags& show) {
    std::string out;
    out.reserve(line.size() + (show.ends ? 1 : 0));
    for (unsigned char c : line) {
        if (c == '\t') {
            out += show.tabs ? "^I" : "\t";
            continue;
        }
        if (c == '\r' && (show.ends || show.nonprinting)) {
            out += "^M";
            continue;
        }
        if (show.nonprinting && (c < 32 || c == 127)) {
            append_caret(out, c);
            continue;
        }
        out += static_cast<char>(c);
    }
    if (show.ends) {
        out += '$';
    }
    return out;
}

std::string number_prefix(int n) {
    std::ostringstream out;
    out << std::setw(6) << n << "  ";
    return out.str();
}

std::size_t tab_width(std::size_t col) {
    const std::size_t w = 8 - (col % 8);
    return w == 0 ? 8 : w;
}

std::vector<std::string> wrap_line(const std::string& line, std::size_t budget) {
    std::vector<std::string> parts;
    if (budget < 1) {
        budget = 1;
    }
    if (line.empty()) {
        parts.emplace_back();
        return parts;
    }

    std::string cur;
    std::size_t cur_w = 0;
    std::size_t i = 0;
    while (i < line.size()) {
        const std::size_t start = i;
        char32_t cp = 0;
        if (!utils::width::next_codepoint(line, i, cp)) {
            break;
        }

        std::size_t w = 0;
        if (cp == U'\t') {
            w = tab_width(cur_w);
        } else {
            const int cw = utils::width::codepoint_width(cp);
            if (cw > 0) {
                w = static_cast<std::size_t>(cw);
            }
        }

        if (w == 0) {
            cur.append(line, start, i - start);
            continue;
        }
        if (cur_w + w > budget && !cur.empty()) {
            parts.push_back(std::move(cur));
            cur.clear();
            cur_w = 0;
            if (cp == U'\t') {
                w = tab_width(0);
            }
        }
        cur.append(line, start, i - start);
        cur_w += w;
    }
    if (!cur.empty() || parts.empty()) {
        parts.push_back(std::move(cur));
    }
    return parts;
}

void emit_numbered(const std::string& line, int n) {
    const std::string prefix = number_prefix(n);
    if (!utils::output::is_stdout_tty()) {
        utils::output::writeln(prefix + line);
        return;
    }

    const auto term = static_cast<std::size_t>(utils::output::terminal_width());
    const std::size_t budget = term > kPrefixCols ? term - kPrefixCols : 1;
    const auto parts = wrap_line(line, budget);
    const std::string pad(kPrefixCols, ' ');
    for (std::size_t i = 0; i < parts.size(); ++i) {
        utils::output::writeln((i == 0 ? prefix : pad) + parts[i]);
    }
}

void emit(const std::string& shown, bool original_blank, bool number_all, bool number_nonblank,
          int& all_no, int& nb_no) {
    if (number_nonblank) {
        if (!original_blank) {
            ++nb_no;
            emit_numbered(shown, nb_no);
        } else {
            utils::output::writeln(shown);
        }
        return;
    }
    if (number_all) {
        ++all_no;
        emit_numbered(shown, all_no);
        return;
    }
    utils::output::writeln(shown);
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("cat", "Concatenate files and print on the standard output");
    parser.flag("n", "number", "number all output lines")
        .flag("b", "number-nonblank", "number nonempty output lines")
        .flag("s", "squeeze-blank", "suppress repeated empty output lines")
        .flag("v", "show-nonprinting", "show ASCII controls as ^; leave other bytes as-is")
        .flag("E", "show-ends", "display $ at end of each line")
        .flag("T", "show-tabs", "display TAB characters as ^I")
        .flag("A", "show-all", "equivalent to -vET")
        .flag("e", "", "equivalent to -vE")
        .flag("", "help", "show this help")
        .positional("FILE", "file to print, or - for stdin", true);

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

    const bool number_nonblank = parsed.has("number-nonblank");
    const bool number_all = parsed.has("number") && !number_nonblank;
    const bool squeeze = parsed.has("squeeze-blank");
    const bool show_all = parsed.has("show-all");
    const bool show_ve = parsed.has("e");
    const ShowFlags show{
        parsed.has("show-nonprinting") || show_all || show_ve,
        parsed.has("show-ends") || show_all || show_ve,
        parsed.has("show-tabs") || show_all,
    };
    const bool keep_cr = show.ends || show.nonprinting;
    auto files = utils::fsutil::expand_globs(parsed.positionals);
    int all_no = 0;
    int nb_no = 0;
    bool had_error = false;
    bool prev_blank = false;

    auto dump_lines = [&](const std::vector<std::string>& lines) {
        for (const auto& line : lines) {
            const bool blank = is_blank(line);
            if (squeeze && blank && prev_blank) {
                continue;
            }
            prev_blank = blank;
            emit(make_visible(line, show), blank, number_all, number_nonblank, all_no, nb_no);
        }
    };

    if (files.empty()) {
        dump_lines(utils::sys::read_stdin_lines(!keep_cr));
        return 0;
    }

    for (const auto& file : files) {
        if (file == "-") {
            dump_lines(utils::sys::read_stdin_lines(!keep_cr));
            continue;
        }
        const auto path = utils::sys::path_from_utf8(file);
        if (!utils::fsutil::exists(path)) {
            utils::output::writeln_err("cat: " + file + ": No such file or directory");
            had_error = true;
            continue;
        }
        if (utils::fsutil::is_dir(path)) {
            utils::output::writeln_err("cat: " + file + ": Is a directory");
            had_error = true;
            continue;
        }
        std::vector<std::string> lines;
        std::string err;
        if (!utils::sys::read_file_lines(path, lines, err, !keep_cr)) {
            utils::output::writeln_err("cat: " + file + ": " + err);
            had_error = true;
            continue;
        }
        dump_lines(lines);
    }
    return had_error ? 1 : 0;
}
