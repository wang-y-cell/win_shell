#include "utils/output.h"
#include "utils/parser.h"
#include "utils/sys.h"

#include <filesystem>
#include <system_error>

int main(int argc, char* argv[]) {
    utils::output::init();
    utils::Parser parser("pwd", "Print the current working directory");
    parser.flag("P", "physical", "print the resolved physical path")
        .flag("", "help", "show this help");

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

    std::error_code ec;
    auto path = std::filesystem::current_path(ec);
    if (ec) {
        utils::output::writeln_err("pwd: " + ec.message());
        return 1;
    }
    if (parsed.has("physical")) {
        path = std::filesystem::canonical(path, ec);
        if (ec) {
            path = std::filesystem::current_path();
        }
    }
    utils::output::writeln(utils::sys::path_to_utf8(path));
    return 0;
}
