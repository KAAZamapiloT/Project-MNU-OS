#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

#include "Config.h"
#include <string>

class ConfigParser {
public:
    // FIX: Renamed this function to match the call in main.cpp
    static bool parse_json(const std::string& filepath, Config& out_config);
    static bool validate(const Config& config);
};

#endif // CONFIG_PARSER_H

