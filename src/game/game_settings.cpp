#include "game_settings.hpp"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <fstream>

namespace game
{

static std::string trim(std::string s)
{
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front()))) {
        s.erase(s.begin());
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back()))) {
        s.pop_back();
    }
    return s;
}

bool loadSettings(GameSettings &out, const std::string &path)
{
    std::ifstream f(path);
    if (!f) {
        return false;
    }

    std::string line;
    while (std::getline(f, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';') {
            continue;
        }
        auto eq = line.find('=');
        if (eq == std::string::npos) {
            continue;
        }
        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));

        char *end = nullptr;
        if (key == "fov") {
            out.fov = std::strtof(val.c_str(), &end);
        } else if (key == "shadows") {
            out.shadows = std::strtol(val.c_str(), &end, 10) != 0;
        } else if (key == "ssao") {
            out.ssao = std::strtol(val.c_str(), &end, 10) != 0;
        } else if (key == "godrays") {
            out.godRays = std::strtol(val.c_str(), &end, 10) != 0;
        } else if (key == "weather") {
            out.weather = std::strtol(val.c_str(), &end, 10) != 0;
        } else if (key == "terrain_preset") {
            out.terrainPreset = static_cast<int>(std::strtol(val.c_str(), &end, 10));
        } else if (key == "time_scale") {
            out.timeScale = std::strtof(val.c_str(), &end);
        } else if (key == "render_distance") {
            out.renderDistanceChunks = static_cast<int>(std::strtol(val.c_str(), &end, 10));
        }
    }

    out.fov = std::clamp(out.fov, 45.0f, 110.0f);
    out.timeScale = std::clamp(out.timeScale, 0.0f, 50.0f);
    if (out.terrainPreset < 0 || out.terrainPreset > 2) {
        out.terrainPreset = 1;
    }
    out.renderDistanceChunks = std::clamp(out.renderDistanceChunks, 2, 24);
    return true;
}

bool saveSettings(const GameSettings &in, const std::string &path)
{
    std::ofstream f(path);
    if (!f) {
        return false;
    }

    f << "# vk-minecraft settings\n";
    f << "fov=" << in.fov << '\n';
    f << "shadows=" << (in.shadows ? 1 : 0) << '\n';
    f << "ssao=" << (in.ssao ? 1 : 0) << '\n';
    f << "godrays=" << (in.godRays ? 1 : 0) << '\n';
    f << "weather=" << (in.weather ? 1 : 0) << '\n';
    f << "terrain_preset=" << in.terrainPreset << '\n';
    f << "time_scale=" << in.timeScale << '\n';
    f << "render_distance=" << in.renderDistanceChunks << '\n';
    return true;
}

} // namespace game
