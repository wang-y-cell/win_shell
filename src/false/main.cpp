#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("false", "Do nothing, unsuccessfully");
    parser.flag("", "help", "show this help");
    const auto args = utils::sys::utf8_argv(argc, argv);
    const auto parsed = parser.parse(args);
    if (parsed.has("help")) {
        utils::output::write(parser.help());
        return 0;
    }
    return 1;
}
