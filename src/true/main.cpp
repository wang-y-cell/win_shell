#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("true", "Do nothing, successfully");
    parser.flag("", "help", "show this help");
    const auto args = utils::sys::utf8_argv(argc, argv);
    const auto parsed = parser.parse(args);
    if (parsed.has("help")) {
        utils::output::write(parser.help());
    }
    return 0;
}
