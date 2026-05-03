#pragma once

#include <glm/ext.hpp>

#include <vector>
#include <memory>
#include <functional>

#include "core/window/window.hpp"
#include "graphics/device.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/texture_cache.hpp"
#include "audio/sound_manager.hpp"
#include "text_renderer.hpp"
#include "game/game_state.hpp"
#include "game/game_settings.hpp"
#include "world/block.hpp"

#include <array>

#include "element.hpp"
#include "button.hpp"

namespace gui
{

struct GameStat
{
    u32 fps = 0;
    u32 updatedChunks = 0;
    game::GameState state = game::GameState::RUNNING;
    game::GameSettings settings{};
    f32 dayPhase01 = 0.35f;

    bool inventoryOpen = false;
    int hotbarSlot = 0;
    std::array<wld::BlockType, 9> hotbar{};
    std::array<wld::BlockType, 27> backpack{};
};

struct PauseSettingsActions
{
    std::function<void()> toggleShadows;
    std::function<void()> toggleSsao;
    std::function<void()> toggleGodRays;
    std::function<void()> toggleWeather;
    std::function<void()> cycleTerrain;
    std::function<void()> fovDecrease;
    std::function<void()> fovIncrease;
    std::function<void()> timeSlower;
    std::function<void()> timeFaster;
    std::function<void()> renderDistanceDecrease;
    std::function<void()> renderDistanceIncrease;
    std::function<void()> saveSettings;
};

class GUI
{

public:
    using CallBackFn = std::function<void()>;

    void init(gfx::Device &device, gfx::TextureCache &textureCache);
    void destroy();

    void update(const glm::vec2 &point);
    void handleMouseClick();
    void updateStat(const GameStat &gameStat) { m_gameStat = gameStat; }

    /// Unified index: 0–8 hotbar, 9–35 main inventory. -1 if none.
    int pickInventorySlot(const glm::vec2 &px) const;

    void render(VkCommandBuffer cmd);

    void initGameElements();
    void initPauseElements();
    void registerPauseSettingsActions(PauseSettingsActions actions);

    void setQuitCallback(CallBackFn callback) { m_quitCallback = callback; }
    void setResumeCallback(CallBackFn callback) { m_resumeCallback = callback; }

    void draw(VkCommandBuffer cmd, const Element &element);

    void drawText(
        VkCommandBuffer cmd,
        const std::string &text,
        const glm::vec2 &pos,
        f32 size,
        TextAlign align = TextAlign::LEFT
    )
    {
        m_text.draw(cmd, text, pos, size, align);
    }

public:
    VkExtent2D getExtent() const
    {
        return m_device->getExtent();
    }

private:
    gfx::Device *m_device = nullptr;

    gfx::Pipeline m_pipeline;

    struct PushConstant
    {
        alignas(16) glm::mat4 model;
        alignas(16) glm::vec4 uv;
        alignas(4) u32 textureID;
    };

    constexpr static u32 ATLAS_SIZE = 256;

    std::unordered_map<std::string, u32> m_textureIDs;

    TextRenderer m_text;

    std::unordered_map<std::string, Element> m_elements;
    std::unordered_map<std::string, std::unique_ptr<Button>> m_buttons;

    GameStat m_gameStat;

    void drawGameElements(VkCommandBuffer cmd);
    void drawPauseElements(VkCommandBuffer cmd);

    CallBackFn m_quitCallback = nullptr;
    CallBackFn m_resumeCallback = nullptr;

    PauseSettingsActions m_pauseSettings;
};

} // namespace gui