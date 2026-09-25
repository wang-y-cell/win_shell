#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("echo", "Write arguments to standard output");
    parser.flag("n", "no-newline", "do not output the trailing newline")
        .flag("", "help", "show this help")
        .positional("STRING", "text to write", true);

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

    std::string out;
    for (std::size_t i = 0; i < parsed.positionals.size(); ++i) {
        if (i) {
            out += ' ';
        }
        out += parsed.positionals[i];
    }
    if (parsed.has("no-newline")) {
        utils::output::write(out);
    } else {
        utils::output::writeln(out);
    }
    return 0;
}
