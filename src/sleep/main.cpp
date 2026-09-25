#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <chrono>
#include <cstdlib>
#include <thread>

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

bool parse_duration(const std::string& text, double& seconds, std::string& error) {
    if (text.empty()) {
        error = "sleep: invalid time interval '" + text + "'";
        return false;
    }
    char* end = nullptr;
    seconds = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || seconds < 0) {
        error = "sleep: invalid time interval '" + text + "'";
        return false;
    }
    if (*end == '\0' || *end == 's' || *end == 'S') {
        return true;
    }
    if (*end == 'm' || *end == 'M') {
        seconds *= 60;
        return true;
    }
    if (*end == 'h' || *end == 'H') {
        seconds *= 3600;
        return true;
    }
    if (*end == 'd' || *end == 'D') {
        seconds *= 86400;
        return true;
    }
    error = "sleep: invalid time interval '" + text + "'";
    return false;
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("sleep", "Delay for a specified amount of time");
    parser.flag("", "help", "show this help").positional("NUMBER", "seconds, or NUMBER[smhd]", true);

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
        utils::output::writeln_err("sleep: missing operand");
        return 1;
    }

    double total = 0;
    for (const auto& tok : parsed.positionals) {
        double part = 0;
        std::string err;
        if (!parse_duration(tok, part, err)) {
            utils::output::writeln_err(err);
            return 1;
        }
        total += part;
    }
#ifdef _WIN32
    Sleep(static_cast<DWORD>(total * 1000.0 + 0.5));
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(static_cast<int>(total * 1000.0 + 0.5)));
#endif
    return 0;
}
