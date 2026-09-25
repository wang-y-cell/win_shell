#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <iostream>

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("yes", "Output a string repeatedly until killed");
    parser.flag("", "help", "show this help").positional("STRING", "string to repeat", true);

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

    std::string text = "y";
    if (!parsed.positionals.empty()) {
        text = parsed.positionals[0];
        for (std::size_t i = 1; i < parsed.positionals.size(); ++i) {
            text += ' ';
            text += parsed.positionals[i];
        }
    }
    while (std::cout) {
        utils::output::writeln(text);
        if (std::cout.fail()) {
            break;
        }
    }
    return 0;
}
