#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace utils {

struct ParseResult {
    bool ok = true;
    std::string error;
    std::unordered_map<std::string, std::string> values; //选项参数,key为参数名,value为参数值,如果没有值也存一个空字符串
    std::vector<std::string> positionals; //位置参数,非选项参数

    bool has(const std::string& name) const;
    std::string get(const std::string& name, const std::string& def = "") const;
    int get_int(const std::string& name, int def = 0) const;
};

class Parser {
public:
    Parser(std::string program, std::string description = "");

    // 添加参数
    Parser& flag(const std::string& short_name,
                 const std::string& long_name,
                 const std::string& help);

    // 添加参数，需要值,value_name是帮助文本中显示的参数名(例如"NUM")
    Parser& option(const std::string& short_name,
                   const std::string& long_name,
                   const std::string& value_name,
                   const std::string& help);

    // 添加位置参数，用于帮助文本中显示的参数名(例如"FILE")
    Parser& positional(const std::string& name,
                       const std::string& help = "",
                       bool repeatable = false);

    ParseResult parse(int argc, char* argv[]) const;
    //解析命令行参数
    ParseResult parse(const std::vector<std::string>& argv) const;

    std::string help() const;
    const std::string& program() const { return program_; }

private:
    struct OptionSpec {
        std::string short_name; //短参数
        std::string long_name; //长参数
        std::string value_name; //值参数
        std::string help; //帮助参数
        bool requires_value = false; //是否需要值
    };

    struct PositionalSpec {
        std::string name;
        std::string help;
        bool repeatable = false;
    };

    std::string program_; //程序名
    std::string description_; //描述
    std::vector<OptionSpec> specs_;
    std::vector<PositionalSpec> positionals_;

    const OptionSpec* find_short(char c) const;
    const OptionSpec* find_long(const std::string& name) const;
    //检验参数是否合格,如果不合格将抛出异常
    void validate_names(const std::string& short_name,
                        const std::string& long_name) const;
    static void store(ParseResult& result,
                      const OptionSpec& spec,
                      const std::string& value);
    static std::string option_label(const OptionSpec& spec);
    static std::string basename(const std::string& path);
};

}  // namespace utils
