#include "gui.hpp"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace gui
{

namespace
{

constexpr f32 kInvSlotPx = 44.0f;
constexpr f32 kInvGapPx = 4.0f;

const char *blockHudLabel(wld::BlockType t)
{
    switch (t) {
    case wld::BlockType::AIR:
        return "-";
    case wld::BlockType::STONE:
        return "Stn";
    case wld::BlockType::GRASS:
        return "Grs";
    case wld::BlockType::DIRT:
        return "Drt";
    case wld::BlockType::COBBLESTONE:
        return "Cbl";
    case wld::BlockType::BEDROCK:
        return "Bd";
    case wld::BlockType::WATER:
        return "H2O";
    case wld::BlockType::SAND:
        return "Snd";
    case wld::BlockType::LOG:
        return "Log";
    case wld::BlockType::LEAVES:
        return "Lf";
    case wld::BlockType::FLOWER:
        return "Flw";
    case wld::BlockType::ROSE:
        return "Ros";
    default:
        return "?";
    }
}

/// Standard 256×256 GUI atlas (minecraft-style layout); UVs trimmed to slot chrome.
constexpr glm::vec4 kAtlasSlotUvPx{225.0f, 238.0f, 38.0f, 38.0f};
constexpr glm::vec4 kAtlasSlotSelectUvPx{213.0f, 234.0f, 50.0f, 50.0f};

void drawInventorySlotSprite(
    GUI &gui,
    VkCommandBuffer cmd,
    Anchor anchor,
    glm::vec2 pos,
    glm::vec2 size,
    glm::vec4 slotUvPx,
    bool highlight
)
{
    Element slotEl = {
        .anchor = anchor,
        .pos = pos,
        .size = size,
        .uv = slotUvPx,
        .texture = "gui",
    };
    gui.draw(cmd, slotEl);
    if (highlight) {
        Element hi = {
            .anchor = anchor,
            .pos = {pos.x - 2.0f, pos.y - 2.0f},
            .size = {size.x + 4.0f, size.y + 4.0f},
            .uv = kAtlasSlotSelectUvPx,
            .texture = "gui",
        };
        gui.draw(cmd, hi);
    }
}

} // namespace

int GUI::pickInventorySlot(const glm::vec2 &px) const
{
    if (!m_gameStat.inventoryOpen) {
        return -1;
    }

    VkExtent2D ext = m_device->getExtent();
    const f32 stride = kInvSlotPx + kInvGapPx;
    const f32 cx = static_cast<f32>(ext.width) * 0.5f;
    const f32 gridW = 9.0f * kInvSlotPx + 8.0f * kInvGapPx;
    const f32 gridH = 3.0f * kInvSlotPx + 2.0f * kInvGapPx;
    const f32 gridLeft = cx - gridW * 0.5f;
    const f32 gridTop =
        static_cast<f32>(ext.height) * 0.5f - gridH * 0.5f - 36.0f;

    for (int row = 0; row < 3; ++row) {
        for (int col = 0; col < 9; ++col) {
            const f32 x0 = gridLeft + static_cast<f32>(col) * stride;
            const f32 y0 = gridTop + static_cast<f32>(row) * stride;
            if (px.x >= x0 && px.x < x0 + kInvSlotPx && px.y >= y0 && px.y < y0 + kInvSlotPx) {
                return 9 + row * 9 + col;
            }
        }
    }

    const f32 hbLeft = gridLeft;
    const f32 hbTop = gridTop + gridH + 20.0f;
    for (int col = 0; col < 9; ++col) {
        const f32 x0 = hbLeft + static_cast<f32>(col) * stride;
        const f32 y0 = hbTop;
        if (px.x >= x0 && px.x < x0 + kInvSlotPx && px.y >= y0 && px.y < y0 + kInvSlotPx) {
            return col;
        }
    }

    return -1;
}

void GUI::init(gfx::Device &device, gfx::TextureCache &textureCache)
{
    m_device = &device;

    m_textureIDs["gui"] = textureCache.getTextureID("gui");
    m_textureIDs["icons"] = textureCache.getTextureID("icons");

    initGameElements();

    m_pipeline = gfx::Pipeline::Builder(device)
        .setShader("gui.vert.spv", VK_SHADER_STAGE_VERTEX_BIT)
        .setShader("gui.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
        .setPushConstant(sizeof(PushConstant))
        .setDepthTest(false)
        .setDepthWrite(false)
        .setBlending(true)
        .build();

    m_text.init(device, textureCache);
}

void GUI::destroy()
{
    m_text.destroy();

    m_pipeline.destroy();
}

void GUI::update(const glm::vec2 &point)
{
    auto state = m_gameStat.state;

    if (state == game::GameState::PAUSED) {
        for (auto &[_, button] : m_buttons) {
            button->update(point);
        }
    }
}

void GUI::handleMouseClick()
{
    for (auto &[_, button] : m_buttons) {
        button->handleMouseClick();
    }
}

void GUI::render(VkCommandBuffer cmd)
{
    switch (m_gameStat.state)
    {

    case game::GameState::RUNNING:
        drawGameElements(cmd);
        break;

    case game::GameState::PAUSED:
        drawPauseElements(cmd);
        break;

    default:
        break;
    }
}

void GUI::initGameElements()
{
    Element crosshair = {
        .anchor = Anchor::CENTER,
        .pos = {0.0f, 0.0f},
        .size = {64.0f, 64.0f},
        .uv = {0.0f, 0.0f, 16.0f, 16.0f},
        .texture = "icons",
    };

    m_elements["crosshair"] = crosshair;
}

void GUI::initPauseElements()
{
    Element resumeBg = {
        .anchor = Anchor::CENTER,
        .pos = {0.0f, 20.0f},
        .size = {800.0f, 80.0f},
        .uv = {0.0f, 0.0f, 0.0f, 0.0f},
        .texture = "gui",
    };

    m_buttons["resume"] = std::make_unique<Button>(
        this,
        "Back to game",
        resumeBg,
        m_resumeCallback
    );

    Element quitBg = resumeBg;
    quitBg.pos.y = 120.0f;

    m_buttons["quit"] = std::make_unique<Button>(
        this,
        "Save and quit to title",
        quitBg,
        m_quitCallback
    );
}

void GUI::registerPauseSettingsActions(PauseSettingsActions actions)
{
    m_pauseSettings = std::move(actions);

    const glm::vec4 btnUv = {0.0f, 66.0f, 200.0f, 20.0f};

    f32 y = -340.0f;
    auto addBtn = [&](const char *key, std::function<void()> fn) {
        Element el = {
            .anchor = Anchor::CENTER,
            .pos = {0.0f, y},
            .size = {300.0f, 40.0f},
            .uv = btnUv,
            .texture = "gui",
        };
        m_buttons[key] = std::make_unique<Button>(
            this,
            "",
            el,
            [fn]() {
                if (fn) {
                    fn();
                }
            }
        );
        y += 44.0f;
    };

    addBtn("set_shadows", m_pauseSettings.toggleShadows);
    addBtn("set_ssao", m_pauseSettings.toggleSsao);
    addBtn("set_godrays", m_pauseSettings.toggleGodRays);
    addBtn("set_weather", m_pauseSettings.toggleWeather);
    addBtn("set_terrain", m_pauseSettings.cycleTerrain);

    Element fovLo = {
        .anchor = Anchor::CENTER,
        .pos = {-155.0f, y},
        .size = {130.0f, 40.0f},
        .uv = btnUv,
        .texture = "gui",
    };
    m_buttons["set_fov_lo"] = std::make_unique<Button>(
        this,
        "FOV -",
        fovLo,
        [this]() {
            if (m_pauseSettings.fovDecrease) {
                m_pauseSettings.fovDecrease();
            }
        }
    );
    Element fovHi = fovLo;
    fovHi.pos.x = 155.0f;
    m_buttons["set_fov_hi"] = std::make_unique<Button>(
        this,
        "FOV +",
        fovHi,
        [this]() {
            if (m_pauseSettings.fovIncrease) {
                m_pauseSettings.fovIncrease();
            }
        }
    );
    y += 44.0f;

    Element tLo = {
        .anchor = Anchor::CENTER,
        .pos = {-155.0f, y},
        .size = {130.0f, 40.0f},
        .uv = btnUv,
        .texture = "gui",
    };
    m_buttons["set_time_lo"] = std::make_unique<Button>(
        this,
        "Time -",
        tLo,
        [this]() {
            if (m_pauseSettings.timeSlower) {
                m_pauseSettings.timeSlower();
            }
        }
    );
    Element tHi = tLo;
    tHi.pos.x = 155.0f;
    m_buttons["set_time_hi"] = std::make_unique<Button>(
        this,
        "Time +",
        tHi,
        [this]() {
            if (m_pauseSettings.timeFaster) {
                m_pauseSettings.timeFaster();
            }
        }
    );
    y += 44.0f;

    Element rdLo = {
        .anchor = Anchor::CENTER,
        .pos = {-155.0f, y},
        .size = {130.0f, 40.0f},
        .uv = btnUv,
        .texture = "gui",
    };
    m_buttons["set_rd_lo"] = std::make_unique<Button>(
        this,
        "Chunks -",
        rdLo,
        [this]() {
            if (m_pauseSettings.renderDistanceDecrease) {
                m_pauseSettings.renderDistanceDecrease();
            }
        }
    );
    Element rdHi = rdLo;
    rdHi.pos.x = 155.0f;
    m_buttons["set_rd_hi"] = std::make_unique<Button>(
        this,
        "Chunks +",
        rdHi,
        [this]() {
            if (m_pauseSettings.renderDistanceIncrease) {
                m_pauseSettings.renderDistanceIncrease();
            }
        }
    );
    y += 44.0f;

    Element saveEl = {
        .anchor = Anchor::CENTER,
        .pos = {0.0f, y},
        .size = {300.0f, 40.0f},
        .uv = btnUv,
        .texture = "gui",
    };
    m_buttons["set_save"] = std::make_unique<Button>(
        this,
        "Save settings now",
        saveEl,
        [this]() {
            if (m_pauseSettings.saveSettings) {
                m_pauseSettings.saveSettings();
            }
        }
    );
}

void GUI::draw(VkCommandBuffer cmd, const Element &element)
{
    VkExtent2D extent = m_device->getExtent();

    glm::vec2 pos = element.pos;

    switch (element.anchor) {
    case Anchor::TOP_LEFT:
        pos = element.pos;
        break;
        
    case Anchor::TOP_RIGHT:
        pos.x = extent.width - element.pos.x - element.size.x;
        pos.y = element.pos.y;
        break;
        
    case Anchor::BOTTOM_LEFT:
        pos.x = element.pos.x;
        pos.y = extent.height - element.pos.y - element.size.y;
        break;
        
    case Anchor::BOTTOM_RIGHT:
        pos.x = extent.width - element.pos.x - element.size.x;
        pos.y = extent.height - element.pos.y - element.size.y;
        break;
        
    case Anchor::CENTER:
        pos.x = (extent.width - element.size.x) / 2.0f + element.pos.x;
        pos.y = (extent.height - element.size.y) / 2.0f + element.pos.y;
        break;
        
    default:
        pos = element.pos;
        break;
    }

    m_pipeline.bind(cmd);

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(
        pos.x,
        pos.y,
        0.0f
    ));

    model = glm::scale(model, glm::vec3(
        element.size.x,
        element.size.y,
        1.0f
    ));

    PushConstant pc;
    pc.model = model;
    pc.uv = {
        element.uv.x,
        element.uv.y,
        element.uv.x + element.uv.z,
        element.uv.y + element.uv.w
    };
    pc.textureID = m_textureIDs[element.texture];

    pc.uv /= ATLAS_SIZE;

    m_pipeline.push(cmd, pc);

    vkCmdDraw(cmd, 6, 1, 0, 0);
}

void GUI::drawGameElements(VkCommandBuffer cmd)
{
    std::string stat = "Minecraft Vulkan Clone ";
    stat += "(" + std::to_string(m_gameStat.fps) + " fps";
    stat += ", " + std::to_string(m_gameStat.updatedChunks) + " chunk updates)";

    m_text.draw(cmd, stat, {10.0f, 10.0f}, 32.0f);

    VkExtent2D ext = m_device->getExtent();
    const f32 cx = static_cast<f32>(ext.width) * 0.5f;
    const f32 stride = kInvSlotPx + kInvGapPx;
    const f32 hbRowW = 9.0f * kInvSlotPx + 8.0f * kInvGapPx;
    const f32 hbLeft = cx - hbRowW * 0.5f;
    const f32 hbBottomInset = 22.0f;

    auto drawHudLabelCentered =
        [&](wld::BlockType t, f32 cxSlotPx, f32 topYSlot) {
            m_text.draw(
                cmd,
                blockHudLabel(t),
                {cxSlotPx, topYSlot + kInvSlotPx * 0.5f - 2.0f},
                11.0f,
                TextAlign::CENTER
            );
        };

    if (!m_gameStat.inventoryOpen) {
        for (int i = 0; i < 9; ++i) {
            const bool sel = (i == m_gameStat.hotbarSlot);
            const glm::vec2 pos{hbLeft + static_cast<f32>(i) * stride, hbBottomInset};
            drawInventorySlotSprite(
                *this,
                cmd,
                Anchor::BOTTOM_LEFT,
                pos,
                {kInvSlotPx, kInvSlotPx},
                kAtlasSlotUvPx,
                sel
            );
            const f32 topY =
                static_cast<f32>(ext.height) - hbBottomInset - kInvSlotPx;
            const f32 cxSlot = hbLeft + static_cast<f32>(i) * stride + kInvSlotPx * 0.5f;
            drawHudLabelCentered(m_gameStat.hotbar[i], cxSlot, topY);
        }
    } else {
        const f32 gridW = 9.0f * kInvSlotPx + 8.0f * kInvGapPx;
        const f32 gridH = 3.0f * kInvSlotPx + 2.0f * kInvGapPx;
        const f32 gridLeft = cx - gridW * 0.5f;
        const f32 gridTop =
            static_cast<f32>(ext.height) * 0.5f - gridH * 0.5f - 36.0f;

        m_text.draw(cmd, "Inventory", {cx, gridTop - 42.0f}, 22.0f, TextAlign::CENTER);
        m_text.draw(
            cmd,
            "Swap: click two slots — E / Esc",
            {cx, gridTop - 18.0f},
            15.0f,
            TextAlign::CENTER
        );

        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 9; ++col) {
                const int idx = row * 9 + col;
                const glm::vec2 pos{
                    gridLeft + static_cast<f32>(col) * stride,
                    gridTop + static_cast<f32>(row) * stride
                };
                drawInventorySlotSprite(
                    *this,
                    cmd,
                    Anchor::TOP_LEFT,
                    pos,
                    {kInvSlotPx, kInvSlotPx},
                    kAtlasSlotUvPx,
                    false
                );
                m_text.draw(
                    cmd,
                    blockHudLabel(m_gameStat.backpack[idx]),
                    {
                        pos.x + kInvSlotPx * 0.5f,
                        pos.y + kInvSlotPx * 0.5f - 2.0f
                    },
                    11.0f,
                    TextAlign::CENTER
                );
            }
        }

        const f32 hbInvTop = gridTop + gridH + 20.0f;
        for (int i = 0; i < 9; ++i) {
            const glm::vec2 pos{gridLeft + static_cast<f32>(i) * stride, hbInvTop};
            const bool sel = (i == m_gameStat.hotbarSlot);
            drawInventorySlotSprite(
                *this,
                cmd,
                Anchor::TOP_LEFT,
                pos,
                {kInvSlotPx, kInvSlotPx},
                kAtlasSlotUvPx,
                sel
            );
            m_text.draw(
                cmd,
                blockHudLabel(m_gameStat.hotbar[i]),
                {
                    pos.x + kInvSlotPx * 0.5f,
                    pos.y + kInvSlotPx * 0.5f - 2.0f
                },
                11.0f,
                TextAlign::CENTER
            );
        }
    }

    for (auto &[_, element] : m_elements) {
        draw(cmd, element);
    }
}

void GUI::drawPauseElements(VkCommandBuffer cmd)
{
    VkExtent2D extent = m_device->getExtent();
    glm::vec2 textPos(
        extent.width / 2.0f,
        extent.height / 2.0f - 400.0f
    );

    m_text.draw(
        cmd,
        "Game menu — keys 1–5, -/+, [ ], ,/. (chunks), click buttons",
        textPos,
        28.0f,
        TextAlign::CENTER
    );

    const auto &s = m_gameStat.settings;
    if (auto it = m_buttons.find("set_shadows"); it != m_buttons.end()) {
        it->second->setText(s.shadows ? "Shadows: ON" : "Shadows: OFF");
    }
    if (auto it = m_buttons.find("set_ssao"); it != m_buttons.end()) {
        it->second->setText(s.ssao ? "SSAO: ON" : "SSAO: OFF");
    }
    if (auto it = m_buttons.find("set_godrays"); it != m_buttons.end()) {
        it->second->setText(s.godRays ? "God rays: ON" : "God rays: OFF");
    }
    if (auto it = m_buttons.find("set_weather"); it != m_buttons.end()) {
        it->second->setText(s.weather ? "Weather: rain" : "Weather: clear");
    }
    if (auto it = m_buttons.find("set_terrain"); it != m_buttons.end()) {
        const char *names[] = {"flat", "default", "mountains"};
        int tp = std::clamp(s.terrainPreset, 0, 2);
        it->second->setText(std::string("Terrain: ") + names[tp]);
    }
    if (auto it = m_buttons.find("set_fov_lo"); it != m_buttons.end()) {
        it->second->setText("FOV - (" + std::to_string(static_cast<int>(s.fov)) + ")");
    }
    if (auto it = m_buttons.find("set_fov_hi"); it != m_buttons.end()) {
        it->second->setText("FOV + (" + std::to_string(static_cast<int>(s.fov)) + ")");
    }
    {
        std::ostringstream ts;
        ts << std::fixed << std::setprecision(2) << s.timeScale;
        const std::string tss = ts.str();
        if (auto it = m_buttons.find("set_time_lo"); it != m_buttons.end()) {
            it->second->setText("Slower day (" + tss + ")");
        }
        if (auto it = m_buttons.find("set_time_hi"); it != m_buttons.end()) {
            it->second->setText("Faster day (" + tss + ")");
        }
    }
    if (auto it = m_buttons.find("set_rd_lo"); it != m_buttons.end()) {
        it->second->setText("Chunks - (" + std::to_string(s.renderDistanceChunks) + ")");
    }
    if (auto it = m_buttons.find("set_rd_hi"); it != m_buttons.end()) {
        it->second->setText("Chunks + (" + std::to_string(s.renderDistanceChunks) + ")");
    }

    std::ostringstream day;
    day << "Day cycle " << static_cast<int>(m_gameStat.dayPhase01 * 100.f) << "%";
    glm::vec2 dayPos(extent.width / 2.0f, extent.height / 2.0f + 200.0f);
    m_text.draw(cmd, day.str(), dayPos, 22.0f, TextAlign::CENTER);

    for (auto &[_, element] : m_elements) {
        draw(cmd, element);
    }

    for (auto &[_, button] : m_buttons) {
        button->draw(cmd);
    }
}

} // namespace gui