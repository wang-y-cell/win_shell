#include "utils/fsutil.h"
#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <regex>
#include <sstream>

namespace {

struct Subst {
    std::regex re;
    std::string repl;
    bool global = false;
    bool print = false;
};

struct Command {
    char kind = 0;  // s, d, p, q
    Subst subst;
};

bool parse_s(const std::string& text, std::size_t& i, Command& cmd, std::string& error) {
    if (i >= text.size() || text[i] != 's') {
        return false;
    }
    ++i;
    if (i >= text.size()) {
        error = "sed: unterminated s command";
        return false;
    }
    const char sep = text[i++];
    std::string pat;
    std::string repl;
    while (i < text.size() && text[i] != sep) {
        if (text[i] == '\\' && i + 1 < text.size()) {
            pat.push_back(text[i++]);
        }
        pat.push_back(text[i++]);
    }
    if (i >= text.size() || text[i] != sep) {
        error = "sed: unterminated s command";
        return false;
    }
    ++i;
    while (i < text.size() && text[i] != sep) {
        if (text[i] == '\\' && i + 1 < text.size()) {
            repl.push_back(text[i++]);
        }
        repl.push_back(text[i++]);
    }
    if (i >= text.size() || text[i] != sep) {
        error = "sed: unterminated s command";
        return false;
    }
    ++i;
    cmd.kind = 's';
    while (i < text.size() && text[i] != ';' && text[i] != '\n') {
        if (text[i] == 'g') {
            cmd.subst.global = true;
        } else if (text[i] == 'p') {
            cmd.subst.print = true;
        }
        ++i;
    }
    try {
        cmd.subst.re = std::regex(pat);
        cmd.subst.repl = repl;
    } catch (const std::regex_error&) {
        error = "sed: invalid regular expression";
        return false;
    }
    return true;
}

bool parse_script(const std::string& text, std::vector<Command>& out, std::string& error) {
    for (std::size_t i = 0; i < text.size();) {
        while (i < text.size() && (text[i] == ' ' || text[i] == '\t' || text[i] == ';' ||
                                   text[i] == '\n' || text[i] == '\r')) {
            ++i;
        }
        if (i >= text.size()) {
            break;
        }
        Command cmd;
        if (text[i] == 's') {
            if (!parse_s(text, i, cmd, error)) {
                return false;
            }
            out.push_back(std::move(cmd));
            continue;
        }
        if (text[i] == 'd' || text[i] == 'p' || text[i] == 'q') {
            cmd.kind = text[i++];
            out.push_back(cmd);
            continue;
        }
        error = "sed: unknown command '" + std::string(1, text[i]) + "'";
        return false;
    }
    return true;
}

std::string apply_subst(const std::string& line, const Subst& s, bool& changed) {
    if (s.global) {
        const auto out = std::regex_replace(line, s.re, s.repl);
        changed = out != line;
        return out;
    }
    std::smatch m;
    if (!std::regex_search(line, m, s.re)) {
        changed = false;
        return line;
    }
    changed = true;
    return line.substr(0, static_cast<std::size_t>(m.position())) + m.format(s.repl) +
           line.substr(static_cast<std::size_t>(m.position() + m.length()));
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("sed", "Stream editor for filtering and transforming text");
    parser.flag("n", "quiet", "suppress automatic printing")
        .option("e", "expression", "SCRIPT", "add the script to commands")
        .option("f", "file", "FILE", "add contents of FILE to commands")
        .flag("", "help", "show this help")
        .positional("SCRIPT", "if no -e/-f, the first argument is the script")
        .positional("FILE", "file to edit", true);

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

    std::vector<std::string> scripts;
    if (parsed.has("expression")) {
        scripts.push_back(parsed.get("expression"));
    }
    if (parsed.has("file")) {
        std::vector<std::string> lines;
        std::string err;
        if (!utils::sys::read_file_lines(utils::sys::path_from_utf8(parsed.get("file")), lines, err)) {
            utils::output::writeln_err("sed: " + err);
            return 1;
        }
        std::ostringstream joined;
        for (const auto& line : lines) {
            joined << line << '\n';
        }
        scripts.push_back(joined.str());
    }

    auto pos = parsed.positionals;
    if (scripts.empty()) {
        if (pos.empty()) {
            utils::output::writeln_err("sed: missing script");
            return 1;
        }
        scripts.push_back(pos[0]);
        pos.erase(pos.begin());
    }

    std::vector<Command> cmds;
    for (const auto& sc : scripts) {
        std::string err;
        if (!parse_script(sc, cmds, err)) {
            utils::output::writeln_err(err);
            return 1;
        }
    }

    const bool quiet = parsed.has("quiet");
    auto files = utils::fsutil::expand_globs(pos);
    std::vector<std::string> lines;
    if (files.empty()) {
        lines = utils::sys::read_stdin_lines();
    } else {
        for (const auto& file : files) {
            std::string err;
            if (!utils::sys::read_file_lines(utils::sys::path_from_utf8(file), lines, err)) {
                utils::output::writeln_err("sed: " + file + ": " + err);
                return 1;
            }
        }
    }

    for (const auto& original : lines) {
        std::string line = original;
        bool deleted = false;
        bool quit = false;
        for (const auto& cmd : cmds) {
            if (cmd.kind == 's') {
                bool changed = false;
                line = apply_subst(line, cmd.subst, changed);
                if (changed && cmd.subst.print) {
                    utils::output::writeln(line);
                }
            } else if (cmd.kind == 'd') {
                deleted = true;
                break;
            } else if (cmd.kind == 'p') {
                utils::output::writeln(line);
            } else if (cmd.kind == 'q') {
                quit = true;
                break;
            }
        }
        if (!deleted && !quiet) {
            utils::output::writeln(line);
        }
        if (quit) {
            break;
        }
    }
    return 0;
}
