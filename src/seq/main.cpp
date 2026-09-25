#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iomanip>
#include <sstream>

namespace {

bool parse_num(const std::string& text, double& value, std::string& error) {
    char* end = nullptr;
    value = std::strtod(text.c_str(), &end);
    if (end == text.c_str() || *end != '\0') {
        error = "seq: invalid floating point argument: " + text;
        return false;
    }
    return true;
}

int width_of(double first, double last, double step) {
    int w = 1;
    auto digits = [](double v) {
        std::ostringstream o;
        o << std::fixed << std::setprecision(0) << std::abs(v);
        return static_cast<int>(o.str().size());
    };
    w = std::max(w, digits(first));
    w = std::max(w, digits(last));
    for (double v = first; (step > 0 && v <= last + 1e-12) || (step < 0 && v >= last - 1e-12);
         v += step) {
        w = std::max(w, digits(v));
        if (std::abs(v - last) < 1e-12) {
            break;
        }
    }
    return w;
}

}  // namespace

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("seq", "Print a sequence of numbers");
    parser.option("s", "separator", "STRING", "use STRING to separate numbers")
        .flag("w", "equal-width", "equalize width by padding with leading zeroes")
        .flag("", "help", "show this help")
        .positional("NUMBER", "LAST, or FIRST LAST, or FIRST INCREMENT LAST", true);

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
        utils::output::writeln_err("seq: missing operand");
        return 1;
    }

    double first = 1;
    double step = 1;
    double last = 0;
    std::string err;
    if (parsed.positionals.size() == 1) {
        if (!parse_num(parsed.positionals[0], last, err)) {
            utils::output::writeln_err(err);
            return 1;
        }
    } else if (parsed.positionals.size() == 2) {
        if (!parse_num(parsed.positionals[0], first, err) ||
            !parse_num(parsed.positionals[1], last, err)) {
            utils::output::writeln_err(err);
            return 1;
        }
    } else {
        if (!parse_num(parsed.positionals[0], first, err) ||
            !parse_num(parsed.positionals[1], step, err) ||
            !parse_num(parsed.positionals[2], last, err)) {
            utils::output::writeln_err(err);
            return 1;
        }
    }
    if (step == 0) {
        utils::output::writeln_err("seq: invalid increment");
        return 1;
    }

    const std::string sep = parsed.has("separator") ? parsed.get("separator") : "\n";
    const bool pad = parsed.has("equal-width");
    const int width = pad ? width_of(first, last, step) : 0;
    bool first_out = true;
    for (double v = first; (step > 0 && v <= last + 1e-9) || (step < 0 && v >= last - 1e-9); v += step) {
        if (!first_out) {
            utils::output::write(sep);
        }
        first_out = false;
        if (pad) {
            std::ostringstream o;
            o << std::setw(width) << std::setfill('0') << static_cast<long long>(v);
            utils::output::write(o.str());
        } else if (std::floor(v) == v) {
            utils::output::write(std::to_string(static_cast<long long>(v)));
        } else {
            utils::output::write(std::to_string(v));
        }
        if (std::abs(v - last) < 1e-9) {
            break;
        }
    }
    if (!first_out) {
        utils::output::writeln("");
    }
    return 0;
}
