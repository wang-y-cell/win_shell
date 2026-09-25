#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <algorithm>
#include <cctype>
#include <string>
#include <vector>

namespace {

bool eq(const std::string& a, const std::string& b, bool ignore_case) {
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

enum class Kind { Equal, Delete, Insert };

struct Op {
    Kind kind;
    int a_line;
    int b_line;
    std::string text;
};

std::vector<Op> diff_ops(const std::vector<std::string>& a, const std::vector<std::string>& b,
                         bool ignore_case) {
    const int n = static_cast<int>(a.size());
    const int m = static_cast<int>(b.size());
    std::vector<Op> ops;
    if (n > 4000 || m > 4000 || static_cast<long long>(n) * m > 4000000) {
        const int maxn = std::max(n, m);
        for (int i = 0; i < maxn; ++i) {
            const bool ha = i < n;
            const bool hb = i < m;
            if (ha && hb && eq(a[i], b[i], ignore_case)) {
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
            if (eq(a[i], b[j], ignore_case)) {
                dp[i][j] = dp[i + 1][j + 1] + 1;
            } else {
                dp[i][j] = std::max(dp[i + 1][j], dp[i][j + 1]);
            }
        }
    }
    int i = 0;
    int j = 0;
    while (i < n && j < m) {
        if (eq(a[i], b[j], ignore_case)) {
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
        .flag("", "help", "show this help")
        .positional("FILE", "two files to compare", true);

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
        if (utils::fsutil::is_dir(path)) {
            utils::output::writeln_err("diff: " + raw + ": Is a directory (directory diff not supported)");
            return 2;
        }
    }

    std::vector<std::string> a;
    std::vector<std::string> b;
    std::string err;
    if (!utils::sys::read_file_lines(p1, a, err) || !utils::sys::read_file_lines(p2, b, err)) {
        utils::output::writeln_err("diff: " + err);
        return 2;
    }

    const auto ops = diff_ops(a, b, parsed.has("ignore-case"));
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
    if (parsed.has("brief")) {
        utils::output::writeln("Files " + files[0] + " and " + files[1] + " differ");
        return 1;
    }
    print_normal(ops);
    return 1;
}
