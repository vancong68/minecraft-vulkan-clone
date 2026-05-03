#pragma once

#include "core/types.hpp"

#include <string>

namespace game
{

struct GameSettings
{
    f32 fov = 85.0f;
    bool shadows = true;
    bool ssao = false;
    bool godRays = true;
    bool weather = false;
    /// 0 = flat, 1 = default, 2 = mountains (affects newly generated chunks)
    int terrainPreset = 1;
    f32 timeScale = 1.0f;
    /// Chunk radius around player (2–24), like Minecraft render distance.
    int renderDistanceChunks = 8;
};

bool loadSettings(GameSettings &out, const std::string &path = "settings.cfg");

bool saveSettings(const GameSettings &in, const std::string &path = "settings.cfg");

} // namespace game
