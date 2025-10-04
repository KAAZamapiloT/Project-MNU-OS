#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include "Config.h"
#include <string>

class ConfigParser {
public:
    static bool parse(const std::string& filepath, Config& out_config);
    static bool validate(const Config& config);
};

#endif // CONFIG_PARSER_H
