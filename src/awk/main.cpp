#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <cctype>
#include <cstdlib>
#include <map>
#include <regex>
#include <sstream>

namespace {

std::vector<std::string> split_fs(const std::string& line, const std::string& fs) {
    std::vector<std::string> fields;
    if (fs.empty()) {
        std::size_t i = 0;
        while (i < line.size()) {
            while (i < line.size() && std::isspace(static_cast<unsigned char>(line[i]))) {
                ++i;
            }
            if (i >= line.size()) {
                break;
            }
            const std::size_t s = i;
            while (i < line.size() && !std::isspace(static_cast<unsigned char>(line[i]))) {
                ++i;
            }
            fields.push_back(line.substr(s, i - s));
        }
        return fields;
    }
    std::string cur;
    for (char c : line) {
        if (fs.size() == 1 && c == fs[0]) {
            fields.push_back(cur);
            cur.clear();
        } else if (fs.size() != 1 && fs.find(c) != std::string::npos) {
            fields.push_back(cur);
            cur.clear();
        } else {
            cur.push_back(c);
        }
    }
    fields.push_back(cur);
    return fields;
}

struct Env {
    std::string line;
    std::vector<std::string> fields;
    int nr = 0;
    std::string fs = " ";
    std::string ofs = " ";
    std::map<std::string, std::string> vars;
};

std::string field(const Env& e, int n) {
    if (n == 0) {
        return e.line;
    }
    if (n < 0 || n > static_cast<int>(e.fields.size())) {
        return {};
    }
    return e.fields[static_cast<std::size_t>(n - 1)];
}

std::string eval_token(const std::string& tok, const Env& e) {
    if (tok == "NR") {
        return std::to_string(e.nr);
    }
    if (tok == "NF") {
        return std::to_string(e.fields.size());
    }
    if (tok == "FS") {
        return e.fs;
    }
    if (!tok.empty() && tok[0] == '$') {
        if (tok == "$NF") {
            return e.fields.empty() ? "" : e.fields.back();
        }
        return field(e, std::atoi(tok.c_str() + 1));
    }
    if (tok.size() >= 2 && ((tok.front() == '"' && tok.back() == '"') ||
                            (tok.front() == '\'' && tok.back() == '\''))) {
        return tok.substr(1, tok.size() - 2);
    }
    auto it = e.vars.find(tok);
    if (it != e.vars.end()) {
        return it->second;
    }
    return tok;
}

std::vector<std::string> tokenize(const std::string& text) {
    std::vector<std::string> out;
    std::string cur;
    for (std::size_t i = 0; i < text.size(); ++i) {
        const char c = text[i];
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (!cur.empty()) {
                out.push_back(cur);
                cur.clear();
            }
            continue;
        }
        if (c == ',') {
            if (!cur.empty()) {
                out.push_back(cur);
                cur.clear();
            }
            out.emplace_back(",");
            continue;
        }
        cur.push_back(c);
    }
    if (!cur.empty()) {
        out.push_back(cur);
    }
    return out;
}

void do_print(const std::vector<std::string>& tokens, std::size_t start, const Env& e) {
    std::string out;
    bool first = true;
    for (std::size_t i = start; i < tokens.size(); ++i) {
        if (tokens[i] == ",") {
            continue;
        }
        if (!first) {
            out += e.ofs;
        }
        first = false;
        out += eval_token(tokens[i], e);
    }
    if (start >= tokens.size()) {
        out = e.line;
    }
    utils::output::writeln(out);
}

void run_action(const std::string& action, Env& e) {
    const auto tokens = tokenize(action);
    if (tokens.empty() || tokens[0] == "print") {
        do_print(tokens, tokens.empty() ? 0 : 1, e);
        return;
    }
    if (tokens[0] == "printf" && tokens.size() >= 2) {
        utils::output::write(eval_token(tokens[1], e));
        return;
    }
}

struct Rule {
    std::string pattern;
    std::string action;
    bool always = false;
    bool regex = false;
};

bool parse_program(const std::string& prog, std::vector<Rule>& rules, std::string& error) {
    std::size_t i = 0;
    while (i < prog.size()) {
        while (i < prog.size() && std::isspace(static_cast<unsigned char>(prog[i]))) {
            ++i;
        }
        if (i >= prog.size()) {
            break;
        }
        Rule r;
        if (prog[i] == '{') {
            r.always = true;
        } else if (prog[i] == '/') {
            ++i;
            while (i < prog.size() && prog[i] != '/') {
                r.pattern.push_back(prog[i++]);
            }
            if (i >= prog.size()) {
                error = "awk: unterminated pattern";
                return false;
            }
            ++i;
            r.regex = true;
        } else {
            while (i < prog.size() && prog[i] != '{') {
                r.pattern.push_back(prog[i++]);
            }
        }
        while (i < prog.size() && std::isspace(static_cast<unsigned char>(prog[i]))) {
            ++i;
        }
        if (i < prog.size() && prog[i] == '{') {
            ++i;
            int depth = 1;
            while (i < prog.size() && depth) {
                if (prog[i] == '{') {
                    ++depth;
                } else if (prog[i] == '}') {
                    --depth;
                    if (depth == 0) {
                        ++i;
                        break;
                    }
                }
                if (depth) {
                    r.action.push_back(prog[i++]);
                }
            }
        } else {
            r.action = "print";
        }
        rules.push_back(std::move(r));
    }
    if (rules.empty()) {
        Rule r;
        r.always = true;
        r.action = "print";
        rules.push_back(r);
    }
    return true;
}

bool match_rule(const Rule& r, const Env& e) {
    if (r.always || r.pattern.empty()) {
        return true;
    }
    if (r.regex) {
        try {
            return std::regex_search(e.line, std::regex(r.pattern));
        } catch (...) {
            return e.line.find(r.pattern) != std::string::npos;
        }
    }
    return e.line.find(r.pattern) != std::string::npos;
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("awk", "Pattern-directed scanning and processing language");
    parser.option("F", "field-separator", "FS", "use FS as the field separator")
        .option("v", "assign", "VAR=VAL", "assign variable")
        .flag("", "help", "show this help")
        .positional("PROGRAM", "awk program")
        .positional("FILE", "file to process", true);

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
        utils::output::writeln_err("awk: missing program");
        return 1;
    }

    Env env;
    if (parsed.has("field-separator")) {
        env.fs = parsed.get("field-separator");
    }
    if (parsed.has("assign")) {
        const auto assign = parsed.get("assign");
        const auto eq = assign.find('=');
        if (eq != std::string::npos) {
            env.vars[assign.substr(0, eq)] = assign.substr(eq + 1);
        }
    }

    std::vector<Rule> rules;
    std::string err;
    if (!parse_program(parsed.positionals[0], rules, err)) {
        utils::output::writeln_err(err);
        return 1;
    }

    auto files = std::vector<std::string>(parsed.positionals.begin() + 1, parsed.positionals.end());
    files = utils::fsutil::expand_globs(files);
    std::vector<std::string> lines;
    if (files.empty()) {
        lines = utils::sys::read_stdin_lines();
    } else {
        for (const auto& file : files) {
            std::string e;
            if (!utils::sys::read_file_lines(utils::sys::path_from_utf8(file), lines, e)) {
                utils::output::writeln_err("awk: " + file + ": " + e);
                return 1;
            }
        }
    }

    for (const auto& line : lines) {
        env.line = line;
        env.fields = split_fs(line, env.fs == " " ? "" : env.fs);
        ++env.nr;
        for (const auto& rule : rules) {
            if (match_rule(rule, env)) {
                run_action(rule.action, env);
            }
        }
    }
    return 0;
}
