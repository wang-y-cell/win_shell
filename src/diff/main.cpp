#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace {

std::string normalize(const std::string& text, bool ignore_case, bool ignore_space) {
    std::string out;
    out.reserve(text.size());
    bool prev_space = false;
    for (unsigned char ch : text) {
        if (ignore_space && std::isspace(ch)) {
            if (!prev_space) {
                out.push_back(' ');
                prev_space = true;
            }
            continue;
        }
        prev_space = false;
        out.push_back(ignore_case ? static_cast<char>(std::tolower(ch)) : static_cast<char>(ch));
    }
    while (!out.empty() && out.back() == ' ') {
        out.pop_back();
    }
    return out;
}

bool eq(const std::string& a, const std::string& b, bool ignore_case, bool ignore_space) {
    if (!ignore_case && !ignore_space) {
        return a == b;
    }
    return normalize(a, ignore_case, ignore_space) == normalize(b, ignore_case, ignore_space);
}

enum class Kind { Equal, Delete, Insert };

struct Op {
    Kind kind;
    int a_line;
    int b_line;
    std::string text;
};

std::vector<Op> diff_ops(const std::vector<std::string>& a, const std::vector<std::string>& b,
                         bool ignore_case, bool ignore_space) {
    const int n = static_cast<int>(a.size());
    const int m = static_cast<int>(b.size());
    std::vector<Op> ops;
    if (n > 4000 || m > 4000 || static_cast<long long>(n) * m > 4000000) {
        const int maxn = std::max(n, m);
        for (int i = 0; i < maxn; ++i) {
            const bool ha = i < n;
            const bool hb = i < m;
            if (ha && hb && eq(a[i], b[i], ignore_case, ignore_space)) {
                ops.push_back({Kind::Equal, i + 1, i + 1, a[i]});
            } else {
                if (ha) {
                    ops.push_back({Kind::Delete, i + 1, std::min(i + 1, m), a[i]});
                }
                if (hb) {
                    ops.push_back({Kind::Insert, std::min(i + 1, n), i + 1, b[i]});
                }
            }
        }
        return ops;
    }

    std::vector<std::vector<int>> dp(n + 1, std::vector<int>(m + 1, 0));
    for (int i = n - 1; i >= 0; --i) {
        for (int j = m - 1; j >= 0; --j) {
            if (eq(a[i], b[j], ignore_case, ignore_space)) {
                dp[i][j] = dp[i + 1][j + 1] + 1;
            } else {
                dp[i][j] = std::max(dp[i + 1][j], dp[i][j + 1]);
            }
        }
    }
    int i = 0;
    int j = 0;
    while (i < n && j < m) {
        if (eq(a[i], b[j], ignore_case, ignore_space)) {
            ops.push_back({Kind::Equal, i + 1, j + 1, a[i]});
            ++i;
            ++j;
        } else if (dp[i + 1][j] >= dp[i][j + 1]) {
            ops.push_back({Kind::Delete, i + 1, j + 1, a[i]});
            ++i;
        } else {
            ops.push_back({Kind::Insert, i + 1, j + 1, b[j]});
            ++j;
        }
    }
    while (i < n) {
        ops.push_back({Kind::Delete, i + 1, j + 1, a[i]});
        ++i;
    }
    while (j < m) {
        ops.push_back({Kind::Insert, i + 1, j + 1, b[j]});
        ++j;
    }
    return ops;
}

std::string range(int a, int b) {
    if (a == b) {
        return std::to_string(a);
    }
    return std::to_string(a) + "," + std::to_string(b);
}

void print_unified(const std::vector<Op>& ops, const std::string& fa, const std::string& fb) {
    utils::output::writeln("--- " + fa);
    utils::output::writeln("+++ " + fb);
    std::size_t i = 0;
    while (i < ops.size()) {
        if (ops[i].kind == Kind::Equal) {
            ++i;
            continue;
        }
        std::size_t start = i;
        while (i < ops.size() && ops[i].kind != Kind::Equal) {
            ++i;
        }
        int a_count = 0;
        int b_count = 0;
        int a_line = 0;
        int b_line = 0;
        for (std::size_t k = start; k < i; ++k) {
            if (ops[k].kind == Kind::Delete) {
                if (a_count == 0) {
                    a_line = ops[k].a_line;
                }
                ++a_count;
            } else if (ops[k].kind == Kind::Insert) {
                if (b_count == 0) {
                    b_line = ops[k].b_line;
                }
                ++b_count;
            }
        }
        if (a_line == 0) {
            a_line = ops[start].a_line;
        }
        if (b_line == 0) {
            b_line = ops[start].b_line;
        }
        utils::output::writeln("@@ -" + std::to_string(a_line) + "," + std::to_string(a_count) +
                               " +" + std::to_string(b_line) + "," + std::to_string(b_count) +
                               " @@");
        for (std::size_t k = start; k < i; ++k) {
            if (ops[k].kind == Kind::Delete) {
                utils::output::writeln("-" + ops[k].text);
            } else {
                utils::output::writeln("+" + ops[k].text);
            }
        }
    }
}

void print_normal(const std::vector<Op>& ops) {
    std::size_t i = 0;
    while (i < ops.size()) {
        if (ops[i].kind == Kind::Equal) {
            ++i;
            continue;
        }
        std::vector<Op> dels;
        std::vector<Op> adds;
        while (i < ops.size() && ops[i].kind != Kind::Equal) {
            if (ops[i].kind == Kind::Delete) {
                dels.push_back(ops[i]);
            } else {
                adds.push_back(ops[i]);
            }
            ++i;
        }
        if (!dels.empty() && adds.empty()) {
            utils::output::writeln(range(dels.front().a_line, dels.back().a_line) + "d" +
                                   std::to_string(dels.front().b_line));
            for (const auto& d : dels) {
                utils::output::writeln("< " + d.text);
            }
        } else if (dels.empty() && !adds.empty()) {
            utils::output::writeln(std::to_string(adds.front().a_line) + "a" +
                                   range(adds.front().b_line, adds.back().b_line));
            for (const auto& a : adds) {
                utils::output::writeln("> " + a.text);
            }
        } else {
            utils::output::writeln(range(dels.front().a_line, dels.back().a_line) + "c" +
                                   range(adds.front().b_line, adds.back().b_line));
            for (const auto& d : dels) {
                utils::output::writeln("< " + d.text);
            }
            utils::output::writeln("---");
            for (const auto& a : adds) {
                utils::output::writeln("> " + a.text);
            }
        }
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("diff", "Compare files line by line");
    parser.flag("q", "brief", "report only when files differ")
        .flag("i", "ignore-case", "ignore case differences")
        .flag("b", "ignore-space-change", "ignore changes in the amount of white space")
        .flag("u", "unified", "output unified diff")
        .flag("r", "recursive", "recursively compare directories")
        .flag("", "help", "show this help")
        .positional("FILE", "two files or directories to compare", true);

    const auto args = utils::sys::utf8_argv(argc, argv);
    const auto parsed = parser.parse(args);
    if (!parsed.ok) {
        utils::output::writeln_err(parsed.error);
        return 2;
    }
    if (parsed.has("help")) {
        utils::output::write(parser.help());
        return 0;
    }
    auto files = utils::fsutil::expand_globs(parsed.positionals);
    if (files.size() < 2) {
        utils::output::writeln_err("diff: missing operand");
        return 2;
    }
    if (files.size() > 2) {
        utils::output::writeln_err("diff: extra operand (only two files supported)");
        return 2;
    }

    const auto p1 = utils::sys::path_from_utf8(files[0]);
    const auto p2 = utils::sys::path_from_utf8(files[1]);
    for (int i = 0; i < 2; ++i) {
        const auto& raw = files[i];
        const auto& path = i == 0 ? p1 : p2;
        if (!utils::fsutil::exists(path)) {
            utils::output::writeln_err("diff: " + raw + ": No such file or directory");
            return 2;
        }
    }

    const bool ignore_case = parsed.has("ignore-case");
    const bool ignore_space = parsed.has("ignore-space-change");
    const bool unified = parsed.has("unified");
    const bool brief = parsed.has("brief");
    const bool recursive = parsed.has("recursive");

    auto compare_files = [&](const std::filesystem::path& left, const std::filesystem::path& right,
                             const std::string& la, const std::string& lb) -> int {
        std::vector<std::string> a;
        std::vector<std::string> b;
        std::string err;
        if (!utils::sys::read_file_lines(left, a, err) ||
            !utils::sys::read_file_lines(right, b, err)) {
            utils::output::writeln_err("diff: " + err);
            return 2;
        }
        const auto ops = diff_ops(a, b, ignore_case, ignore_space);
        bool differ = false;
        for (const auto& op : ops) {
            if (op.kind != Kind::Equal) {
                differ = true;
                break;
            }
        }
        if (!differ) {
            return 0;
        }
        if (brief) {
            utils::output::writeln("Files " + la + " and " + lb + " differ");
            return 1;
        }
        if (unified) {
            print_unified(ops, la, lb);
        } else {
            print_normal(ops);
        }
        return 1;
    };

    if (utils::fsutil::is_dir(p1) || utils::fsutil::is_dir(p2)) {
        if (!recursive || !utils::fsutil::is_dir(p1) || !utils::fsutil::is_dir(p2)) {
            utils::output::writeln_err("diff: " + files[0] + ": Is a directory");
            return 2;
        }
        int status = 0;
        std::error_code ec;
        std::vector<std::string> names;
        for (auto it = std::filesystem::recursive_directory_iterator(
                 p1, std::filesystem::directory_options::skip_permission_denied, ec);
             it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
            if (ec) {
                ec.clear();
                continue;
            }
            if (!it->is_regular_file(ec)) {
                continue;
            }
            const auto rel = std::filesystem::relative(it->path(), p1, ec);
            names.push_back(utils::sys::path_to_utf8(rel));
        }
        std::sort(names.begin(), names.end());
        for (const auto& name : names) {
            const auto right = p2 / utils::sys::path_from_utf8(name);
            if (!utils::fsutil::exists(right)) {
                utils::output::writeln("Only in " + files[0] + ": " + name);
                status = 1;
                continue;
            }
            if (utils::fsutil::is_dir(right)) {
                continue;
            }
            const int rc = compare_files(p1 / utils::sys::path_from_utf8(name), right,
                                         files[0] + "/" + name, files[1] + "/" + name);
            if (rc == 2) {
                return 2;
            }
            if (rc == 1) {
                status = 1;
            }
        }
        return status;
    }

    return compare_files(p1, p2, files[0], files[1]);
}
