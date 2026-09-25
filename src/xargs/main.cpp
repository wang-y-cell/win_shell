#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

#ifdef _WIN32
std::wstring quote_win(const std::wstring& text) {
    std::wstring out = L"\"";
    for (wchar_t c : text) {
        if (c == L'"') {
            out += L'\\';
        }
        out += c;
    }
    out += L'"';
    return out;
}

int run_cmd(const std::vector<std::string>& parts, bool verbose) {
    std::wstring cmd;
    for (std::size_t i = 0; i < parts.size(); ++i) {
        if (i) {
            cmd += L' ';
        }
        cmd += quote_win(utils::sys::utf8_to_wide(parts[i]));
    }
    if (verbose) {
        utils::output::writeln_err(utils::sys::wide_to_utf8(cmd.c_str()));
    }
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(L'\0');
    if (!CreateProcessW(nullptr, buf.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
        utils::output::writeln_err("xargs: cannot run command");
        return 1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return static_cast<int>(code);
}
#endif

std::vector<std::string> read_tokens(bool nul) {
    std::vector<std::string> tokens;
    if (nul) {
        std::string data;
        char ch;
        std::string cur;
        while (std::cin.get(ch)) {
            if (ch == '\0') {
                if (!cur.empty()) {
                    tokens.push_back(cur);
                    cur.clear();
                }
            } else {
                cur.push_back(ch);
            }
        }
        if (!cur.empty()) {
            tokens.push_back(cur);
        }
        return tokens;
    }
    for (const auto& line : utils::sys::read_stdin_lines()) {
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
            tokens.push_back(line.substr(s, i - s));
        }
    }
    return tokens;
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("xargs", "Build and execute command lines from standard input");
    parser.option("n", "max-args", "N", "use at most N arguments per command line")
        .option("s", "max-chars", "N", "use at most N characters per command line")
        .flag("t", "verbose", "print commands before executing")
        .flag("x", "exit", "exit if the size is exceeded")
        .flag("0", "null", "items are separated by a null, not whitespace")
        .flag("", "help", "show this help")
        .positional("COMMAND", "command to run")
        .positional("ARG", "initial arguments", true);

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

    std::vector<std::string> prefix = parsed.positionals;
    if (prefix.empty()) {
        prefix.emplace_back("echo");
    }
    const int max_args = parsed.has("max-args") ? parsed.get_int("max-args", 0) : 0;
    const int max_chars = parsed.has("max-chars") ? parsed.get_int("max-chars", 0) : 0;
    const bool verbose = parsed.has("verbose");
    const bool exit_big = parsed.has("exit");
    const auto tokens = read_tokens(parsed.has("null"));

    int status = 0;
    std::size_t i = 0;
    while (i < tokens.size()) {
        std::vector<std::string> cmd = prefix;
        std::size_t chars = 0;
        for (const auto& p : prefix) {
            chars += p.size() + 1;
        }
        int added = 0;
        while (i < tokens.size()) {
            const auto& tok = tokens[i];
            if (max_args > 0 && added >= max_args) {
                break;
            }
            if (max_chars > 0 && chars + tok.size() + 1 > static_cast<std::size_t>(max_chars)) {
                if (added == 0 && exit_big) {
                    utils::output::writeln_err("xargs: argument list too long");
                    return 1;
                }
                break;
            }
            cmd.push_back(tok);
            chars += tok.size() + 1;
            ++added;
            ++i;
        }
#ifdef _WIN32
        const int rc = run_cmd(cmd, verbose);
        if (rc != 0) {
            status = rc;
        }
#else
        (void)verbose;
        (void)cmd;
#endif
    }
    return status;
}
