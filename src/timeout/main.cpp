#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <cstdlib>
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

bool parse_duration(const std::string& text, DWORD& ms, std::string& error) {
    char* end = nullptr;
    const double n = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || n < 0) {
        error = "timeout: invalid time interval '" + text + "'";
        return false;
    }
    double seconds = n;
    if (*end == 'm' || *end == 'M') {
        seconds *= 60;
    } else if (*end == 'h' || *end == 'H') {
        seconds *= 3600;
    } else if (*end == 'd' || *end == 'D') {
        seconds *= 86400;
    } else if (*end != '\0' && *end != 's' && *end != 'S') {
        error = "timeout: invalid time interval '" + text + "'";
        return false;
    }
    ms = static_cast<DWORD>(seconds * 1000.0 + 0.5);
    return true;
}

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
#endif

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("timeout", "Run a command with a time limit");
    parser.flag("", "help", "show this help")
        .positional("DURATION", "number of seconds, or NUMBER[smhd]")
        .positional("COMMAND", "command to run")
        .positional("ARG", "command arguments", true);

    const auto args = utils::sys::utf8_argv(argc, argv);
    if (args.size() >= 2 && (args[1] == "--help" || args[1] == "-h")) {
        utils::output::write(parser.help());
        return 0;
    }
    if (args.size() < 3) {
        utils::output::writeln_err("timeout: missing operand");
        utils::output::write(parser.help());
        return 1;
    }

    DWORD ms = 0;
    std::string err;
    if (!parse_duration(args[1], ms, err)) {
        utils::output::writeln_err(err);
        return 125;
    }

#ifdef _WIN32
    std::wstring cmd;
    for (std::size_t i = 2; i < args.size(); ++i) {
        if (i > 2) {
            cmd += L' ';
        }
        cmd += quote_win(utils::sys::utf8_to_wide(args[i]));
    }
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(L'\0');
    if (!CreateProcessW(nullptr, buf.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi)) {
        utils::output::writeln_err("timeout: failed to run command");
        return 127;
    }
    const DWORD wait = WaitForSingleObject(pi.hProcess, ms);
    if (wait == WAIT_TIMEOUT) {
        TerminateProcess(pi.hProcess, 124);
        WaitForSingleObject(pi.hProcess, INFINITE);
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        return 124;
    }
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    return static_cast<int>(code);
#else
    (void)ms;
    utils::output::writeln_err("timeout: not supported");
    return 125;
#endif
}
