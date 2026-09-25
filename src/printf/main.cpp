#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <string>

namespace {

std::string unescape(const std::string& text) {
    std::string out;
    for (std::size_t i = 0; i < text.size(); ++i) {
        if (text[i] != '\\' || i + 1 >= text.size()) {
            out.push_back(text[i]);
            continue;
        }
        const char n = text[++i];
        switch (n) {
            case 'n':
                out.push_back('\n');
                break;
            case 't':
                out.push_back('\t');
                break;
            case 'r':
                out.push_back('\r');
                break;
            case 'a':
                out.push_back('\a');
                break;
            case 'b':
                out.push_back('\b');
                break;
            case '\\':
                out.push_back('\\');
                break;
            case '"':
                out.push_back('"');
                break;
            default:
                out.push_back(n);
                break;
        }
    }
    return out;
}

std::string format_one(char spec, const std::string& arg) {
    char buf[128];
    switch (spec) {
        case 's':
            return arg;
        case 'c':
            return arg.empty() ? std::string() : std::string(1, arg[0]);
        case 'd':
        case 'i':
            std::snprintf(buf, sizeof(buf), "%d", std::atoi(arg.c_str()));
            return buf;
        case 'u':
            std::snprintf(buf, sizeof(buf), "%u", static_cast<unsigned>(std::strtoul(arg.c_str(), nullptr, 10)));
            return buf;
        case 'x':
            std::snprintf(buf, sizeof(buf), "%x", static_cast<unsigned>(std::strtoul(arg.c_str(), nullptr, 0)));
            return buf;
        case 'X':
            std::snprintf(buf, sizeof(buf), "%X", static_cast<unsigned>(std::strtoul(arg.c_str(), nullptr, 0)));
            return buf;
        case 'o':
            std::snprintf(buf, sizeof(buf), "%o", static_cast<unsigned>(std::strtoul(arg.c_str(), nullptr, 0)));
            return buf;
        case 'f':
            std::snprintf(buf, sizeof(buf), "%f", std::atof(arg.c_str()));
            return buf;
        default:
            return std::string("%") + spec;
    }
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("printf", "Format and print data");
    parser.flag("", "help", "show this help")
        .positional("FORMAT", "format string")
        .positional("ARG", "format argument", true);

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
        utils::output::writeln_err("printf: missing operand");
        return 1;
    }

    const std::string fmt = unescape(parsed.positionals[0]);
    std::size_t argi = 1;
    std::string out;
    for (std::size_t i = 0; i < fmt.size(); ++i) {
        if (fmt[i] != '%' || i + 1 >= fmt.size()) {
            out.push_back(fmt[i]);
            continue;
        }
        ++i;
        if (fmt[i] == '%') {
            out.push_back('%');
            continue;
        }
        const std::string arg = argi < parsed.positionals.size() ? parsed.positionals[argi++] : "";
        out += format_one(fmt[i], arg);
    }
    utils::output::write(out);
    return 0;
}
