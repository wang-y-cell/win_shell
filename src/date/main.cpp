#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <ctime>
#include <cwchar>
#include <string>

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

std::string format_time(const std::tm& t, const std::string& fmt) {
#ifdef _WIN32
    const std::wstring wfmt = utils::sys::utf8_to_wide(fmt);
    wchar_t buf[512];
    if (std::wcsftime(buf, sizeof(buf) / sizeof(buf[0]), wfmt.c_str(), &t) == 0) {
        return {};
    }
    return utils::sys::wide_to_utf8(buf);
#else
    char buf[512];
    if (std::strftime(buf, sizeof(buf), fmt.c_str(), &t) == 0) {
        return {};
    }
    return buf;
#endif
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("date", "Print the current date and time");
    parser.flag("u", "utc", "print Coordinated Universal Time")
        .flag("", "help", "show this help")
        .positional("FORMAT", "+FORMAT string", true);

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

    std::string fmt = "%a %b %d %H:%M:%S %Z %Y";
    if (!parsed.positionals.empty()) {
        const auto& raw = parsed.positionals[0];
        if (raw.empty() || raw[0] != '+') {
            utils::output::writeln_err("date: invalid format '" + raw + "' (use +FORMAT)");
            return 1;
        }
        fmt = raw.substr(1);
    }

    const std::time_t now = std::time(nullptr);
    std::tm t{};
#ifdef _WIN32
    if (parsed.has("utc")) {
        gmtime_s(&t, &now);
    } else {
        localtime_s(&t, &now);
    }
#else
    if (parsed.has("utc")) {
        t = *std::gmtime(&now);
    } else {
        t = *std::localtime(&now);
    }
#endif
    utils::output::writeln(format_time(t, fmt));
    return 0;
}
