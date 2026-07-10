#include "config.h"
#include <SDL.h>
#include <fstream>
#include <sstream>
#include <string>
#include <unordered_map>

namespace
{
    std::string trim(const std::string& s)
    {
        size_t start = s.find_first_not_of(" \t\r\n");
        if (start == std::string::npos) return "";
        size_t end = s.find_last_not_of(" \t\r\n");
        return s.substr(start, end - start + 1);
    }
}

StreamFieldConfig loadConfig(const char* fileName)
{
    StreamFieldConfig config; // defaults

    char* basePath = SDL_GetBasePath();
    std::string path = std::string(basePath ? basePath : "./") + fileName;
    if (basePath) SDL_free(basePath);

    std::ifstream file(path);
    if (!file.is_open())
    {
        SDL_Log("No config file at '%s', using defaults", path.c_str());
        return config;
    }

    std::unordered_map<std::string, std::string> values;
    std::string line;
    while (std::getline(file, line))
    {
        std::string trimmed = trim(line);
        if (trimmed.empty() || trimmed[0] == '#') continue;

        size_t eq = trimmed.find('=');
        if (eq == std::string::npos) continue;

        std::string key = trim(trimmed.substr(0, eq));
        std::string value = trim(trimmed.substr(eq + 1));
        values[key] = value;
    }

    auto getInt = [&](const char* key, int fallback) -> int
    {
        auto it = values.find(key);
        if (it == values.end()) return fallback;
        try { return std::stoi(it->second); }
        catch (...) { return fallback; }
    };

    config.maxStreams = getInt("MaxStream", config.maxStreams);
    config.backTrace  = getInt("BackTrace", config.backTrace);
    config.leading    = getInt("Leading", config.leading);
    config.spacePad   = getInt("SpacePad", config.spacePad);
    config.speedDelay = getInt("SpeedDelay", config.speedDelay);
    config.r = static_cast<Uint8>(getInt("ColorR", config.r));
    config.g = static_cast<Uint8>(getInt("ColorG", config.g));
    config.b = static_cast<Uint8>(getInt("ColorB", config.b));

    return config;
}
