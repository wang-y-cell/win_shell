#include "../../include/utils/parser.h"

#include <cctype>
#include <sstream>
#include <stdexcept>

namespace utils {
namespace {

std::string quote_short(char c) {
    std::string s = "invalid option -- '";
    s.push_back(c);
    s.push_back('\'');
    return s;
}

}  // namespace

bool ParseResult::has(const std::string& name) const {
    return values.find(name) != values.end();
}

std::string ParseResult::get(const std::string& name, const std::string& def) const {
    auto it = values.find(name);
    if (it == values.end()) {
        return def;
    }
    return it->second;
}

int ParseResult::get_int(const std::string& name, int def) const {
    auto it = values.find(name);
    if (it == values.end() || it->second.empty()) {
        return def;
    }
    try {
        std::size_t idx = 0;
        const int value = std::stoi(it->second, &idx);
        if (idx != it->second.size()) {
            return def;
        }
        return value;
    } catch (...) {
        return def;
    }
}

Parser::Parser(std::string program, std::string description)
    : program_(std::move(program)), description_(std::move(description)) {}

Parser& Parser::flag(const std::string& short_name,
                     const std::string& long_name,
                     const std::string& help) {
    validate_names(short_name, long_name);
    specs_.push_back(OptionSpec{short_name, long_name, "", help, false});
    return *this;
}

Parser& Parser::option(const std::string& short_name,
                       const std::string& long_name,
                       const std::string& value_name,
                       const std::string& help) {
    validate_names(short_name, long_name);
    specs_.push_back(OptionSpec{
        short_name, long_name, value_name.empty() ? "VALUE" : value_name, help, true});
    return *this;
}

Parser& Parser::positional(const std::string& name,
                           const std::string& help,
                           bool repeatable) {
    if (name.empty()) {
        throw std::invalid_argument("positional name must not be empty");
    }
    positionals_.push_back(PositionalSpec{name, help, repeatable});
    return *this;
}

ParseResult Parser::parse(int argc, char* argv[]) const {
    std::vector<std::string> args;
    args.reserve(argc > 0 ? static_cast<std::size_t>(argc) : 0);
    for (int i = 0; i < argc; ++i) {
        args.emplace_back(argv[i] ? argv[i] : "");
    }
    return parse(args);
}

ParseResult Parser::parse(const std::vector<std::string>& argv) const {
    ParseResult result;
    const std::string prog =
        program_.empty() ? (argv.empty() ? "" : basename(argv[0])) : program_;

    auto fail = [&](const std::string& message) -> ParseResult {
        result.ok = false;
        result.error = prog.empty() ? message : prog + ": " + message;
        return result;
    };

    const std::size_t start = argv.empty() ? 0 : 1;
    bool options_ended = false;

    for (std::size_t i = start; i < argv.size(); ++i) {
        const std::string& token = argv[i];

        if (options_ended || token == "-" || token.empty() || token[0] != '-') {
            result.positionals.push_back(token);
            continue;
        }

        if (token == "--") {
            options_ended = true;
            continue;
        }

        if (token.rfind("--", 0) == 0) {
            const std::string body = token.substr(2);
            const std::size_t eq = body.find('=');
            const std::string name = eq == std::string::npos ? body : body.substr(0, eq);
            if (name.empty()) {
                return fail("unrecognized option '--'");
            }

            const OptionSpec* spec = find_long(name);
            if (spec == nullptr) {
                return fail("unrecognized option '--" + name + "'");
            }

            if (spec->requires_value) {
                std::string value;
                if (eq != std::string::npos) {
                    value = body.substr(eq + 1);
                } else if (i + 1 < argv.size()) {
                    value = argv[++i];
                } else {
                    return fail("option '--" + name + "' requires an argument");
                }
                store(result, *spec, value);
            } else if (eq != std::string::npos) {
                return fail("option '--" + name + "' doesn't allow an argument");
            } else {
                store(result, *spec, "1");
            }
            continue;
        }

        //解析短参数,注意这里可以解析多个短参数,例如"-abc"会被解析为"-a -b -c"
        //但是如果参数需要值,请将需要值得参数放在最后,例如"-ab value",其中b参数需要值
        //如果没有放在最后,后面得参数就会被忽略
        const std::string cluster = token.substr(1);
        for (std::size_t k = 0; k < cluster.size(); ++k) {
            const char c = cluster[k];
            const OptionSpec* spec = find_short(c);
            if (spec == nullptr) {
                return fail(quote_short(c));
            }

            if (spec->requires_value) {
                std::string value;
                if (k + 1 < cluster.size()) {
                    value = cluster.substr(k + 1);
                } else if (i + 1 < argv.size()) {
                    value = argv[++i];
                } else {
                    std::string msg = "option requires an argument -- '";
                    msg.push_back(c);
                    msg.push_back('\'');
                    return fail(msg);
                }
                store(result, *spec, value);
                break;
            }
            store(result, *spec, "1");
        }
    }

    return result;
}

std::string Parser::help() const {
    std::ostringstream out;
    out << "Usage: " << (program_.empty() ? "command" : program_);
    if (!specs_.empty()) {
        out << " [OPTION]...";
    }
    for (const auto& pos : positionals_) {
        out << " [" << pos.name << "]";
        if (pos.repeatable) {
            out << "...";
        }
    }
    out << '\n';

    if (!description_.empty()) {
        out << '\n' << description_ << '\n';
    }

    std::vector<std::string> labels;
    labels.reserve(specs_.size());
    std::size_t width = 0;
    for (const auto& spec : specs_) {
        labels.push_back(option_label(spec));
        if (labels.back().size() > width) {
            width = labels.back().size();
        }
    }

    if (!specs_.empty()) {
        out << "\nOptions:\n";
        for (std::size_t i = 0; i < specs_.size(); ++i) {
            out << "  " << labels[i];
            if (!specs_[i].help.empty()) {
                out << std::string(width - labels[i].size() + 2, ' ') << specs_[i].help;
            }
            out << '\n';
        }
    }

    bool any_pos_help = false;
    for (const auto& pos : positionals_) {
        if (!pos.help.empty()) {
            any_pos_help = true;
            break;
        }
    }
    if (any_pos_help) {
        out << "\nArguments:\n";
        std::size_t pos_width = 0;
        for (const auto& pos : positionals_) {
            if (pos.name.size() > pos_width) {
                pos_width = pos.name.size();
            }
        }
        for (const auto& pos : positionals_) {
            out << "  " << pos.name;
            if (!pos.help.empty()) {
                out << std::string(pos_width - pos.name.size() + 2, ' ') << pos.help;
            }
            out << '\n';
        }
    }

    return out.str();
}

const Parser::OptionSpec* Parser::find_short(char c) const {
    for (const auto& spec : specs_) {
        if (spec.short_name.size() == 1 && spec.short_name[0] == c) {
            return &spec;
        }
    }
    return nullptr;
}

const Parser::OptionSpec* Parser::find_long(const std::string& name) const {
    for (const auto& spec : specs_) {
        if (!spec.long_name.empty() && spec.long_name == name) {
            return &spec;
        }
    }
    return nullptr;
}

void Parser::validate_names(const std::string& short_name,
                            const std::string& long_name) const {
    if (short_name.empty() && long_name.empty()) {
        throw std::invalid_argument("option must have a short or long name");
    }
    if (!short_name.empty()) {
        if (short_name.size() != 1 || short_name[0] == '-' || std::isspace(
                static_cast<unsigned char>(short_name[0]))) {
            throw std::invalid_argument("short option must be a single non-dash character");
        }
        if (find_short(short_name[0]) != nullptr) {
            throw std::invalid_argument("duplicate short option: -" + short_name);
        }
    }
    if (!long_name.empty()) {
        if (long_name[0] == '-' || long_name.find('=') != std::string::npos ||
            long_name.find(' ') != std::string::npos) {
            throw std::invalid_argument("invalid long option name: " + long_name);
        }
        if (find_long(long_name) != nullptr) {
            throw std::invalid_argument("duplicate long option: --" + long_name);
        }
    }
}

void Parser::store(ParseResult& result,
                   const OptionSpec& spec,
                   const std::string& value) {
    //即使我的参数只是一个短参数或者一个长参数,我也会将他们另一个作为key存入values中
    if (!spec.long_name.empty()) {
        result.values[spec.long_name] = value;
    }
    if (!spec.short_name.empty()) {
        result.values[spec.short_name] = value;
    }
}

std::string Parser::option_label(const OptionSpec& spec) {
    std::string label;
    if (!spec.short_name.empty() && !spec.long_name.empty()) {
        label = "-" + spec.short_name + ", --" + spec.long_name;
    } else if (!spec.long_name.empty()) {
        label = "    --" + spec.long_name;
    } else {
        label = "-" + spec.short_name;
    }
    if (spec.requires_value) {
        label += " " + spec.value_name;
    }
    return label;
}

std::string Parser::basename(const std::string& path) {
    const std::size_t slash = path.find_last_of("\\/");
    return slash == std::string::npos ? path : path.substr(slash + 1);
}

}  // namespace utils
