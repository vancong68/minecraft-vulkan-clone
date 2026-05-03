#include "game.hpp"

#include <algorithm>
#include <cmath>
#include <utility>

namespace game
{

namespace
{

wld::BlockType *unifiedInvSlot(cmp::Player *p, int u)
{
    if (u >= 0 && u < 9) {
        return &p->hotbar[static_cast<size_t>(u)];
    }
    if (u >= 9 && u < 36) {
        return &p->backpack[static_cast<size_t>(u - 9)];
    }
    return nullptr;
}

/// Cap internal 3D resolution so maximize/fullscreen stays responsive on laptops.
constexpr u64 kMaxRenderPixels = 1280ull * 720ull;

std::pair<u32, u32> cappedRenderExtent(u32 windowW, u32 windowH)
{
    if (windowW == 0 || windowH == 0) {
        return {0, 0};
    }
    const u64 pix =
        static_cast<u64>(windowW) * static_cast<u64>(windowH);
    if (pix <= kMaxRenderPixels) {
        return {windowW, windowH};
    }
    const f64 s =
        std::sqrt(static_cast<f64>(kMaxRenderPixels) / static_cast<f64>(pix));
    const u32 rw = std::max(1u, static_cast<u32>(std::floor(static_cast<f64>(windowW) * s)));
    const u32 rh = std::max(1u, static_cast<u32>(std::floor(static_cast<f64>(windowH) * s)));
    return {rw, rh};
}

} // namespace

void Game::syncSwapchainAndDisplay()
{
    int fw = 0;
    int fh = 0;
    glfwGetFramebufferSize(m_window.get(), &fw, &fh);
    if (fw <= 0 || fh <= 0) {
        return;
    }

    VkExtent2D swapExt = m_device.getExtent();
    const u32 uw = static_cast<u32>(fw);
    const u32 uh = static_cast<u32>(fh);

    const bool swapMismatch =
        uw != swapExt.width || uh != swapExt.height || m_window.isResized();

    if (swapMismatch) {
        m_device.waitIdle();
        m_device.recreateSwapchain();
        m_window.clearResizeFlag();
    }

    const auto [rw, rh] = cappedRenderExtent(uw, uh);
    if (rw == 0 || rh == 0) {
        return;
    }

    if (rw != m_display.framebufferWidth() || rh != m_display.framebufferHeight()) {
        m_device.waitIdle();
        m_display.resize(rw, rh);
    }
}

void Game::handlePausedSettingsInput()
{
    if (m_state != GameState::PAUSED) {
        return;
    }

    if (m_window.isKeyJustPressed(GLFW_KEY_1)) {
        m_settings.shadows = !m_settings.shadows;
    }
    if (m_window.isKeyJustPressed(GLFW_KEY_2)) {
        m_settings.ssao = !m_settings.ssao;
    }
    if (m_window.isKeyJustPressed(GLFW_KEY_3)) {
        m_settings.godRays = !m_settings.godRays;
    }
    if (m_window.isKeyJustPressed(GLFW_KEY_4)) {
        m_settings.weather = !m_settings.weather;
    }
    if (m_window.isKeyJustPressed(GLFW_KEY_5)) {
        m_settings.terrainPreset = (m_settings.terrainPreset + 1) % 3;
        m_world.setTerrainPreset(m_settings.terrainPreset);
    }

    if (m_window.isKeyJustPressed(GLFW_KEY_MINUS)
        || m_window.isKeyJustPressed(GLFW_KEY_KP_SUBTRACT)) {
        m_settings.fov -= 5.0f;
        m_settings.fov = std::clamp(m_settings.fov, 45.0f, 110.0f);
        m_camera.setFov(m_settings.fov);
    }
    if (m_window.isKeyJustPressed(GLFW_KEY_EQUAL)
        || m_window.isKeyJustPressed(GLFW_KEY_KP_ADD)) {
        m_settings.fov += 5.0f;
        m_settings.fov = std::clamp(m_settings.fov, 45.0f, 110.0f);
        m_camera.setFov(m_settings.fov);
    }

    if (m_window.isKeyJustPressed(GLFW_KEY_LEFT_BRACKET)) {
        m_settings.timeScale *= 0.75f;
        m_settings.timeScale = std::clamp(m_settings.timeScale, 0.0f, 50.0f);
    }
    if (m_window.isKeyJustPressed(GLFW_KEY_RIGHT_BRACKET)) {
        m_settings.timeScale *= 1.333333f;
        m_settings.timeScale = std::clamp(m_settings.timeScale, 0.0f, 50.0f);
    }

    if (m_window.isKeyJustPressed(GLFW_KEY_COMMA)) {
        m_settings.renderDistanceChunks =
            std::clamp(m_settings.renderDistanceChunks - 1, 2, 24);
        m_world.setRenderDistance(m_settings.renderDistanceChunks);
    }
    if (m_window.isKeyJustPressed(GLFW_KEY_PERIOD)) {
        m_settings.renderDistanceChunks =
            std::clamp(m_settings.renderDistanceChunks + 1, 2, 24);
        m_world.setRenderDistance(m_settings.renderDistanceChunks);
    }
}


Game::Game() :
    m_playerSystem(&m_ecs, m_window, m_camera, m_world, m_overlay),
    m_physicsSystem(&m_ecs, m_world)
{
}

void Game::init()
{
    m_window.init(1600, 900, "Minecraft Clone");

    loadSettings(m_settings);
    
    m_device.init(m_window, "Minecraft Clone", {0, 1, 0});
    m_gpuData.init(m_device);
    m_textureCache.init(m_device);

    m_textureCache.loadTexture("terrain.png", "terrain");
    m_textureCache.loadTexture("font.png", "font");
    m_textureCache.loadTexture("water.png", "water");
    m_textureCache.loadTexture("gui/gui.png", "gui");
    m_textureCache.loadTexture("gui/icons.png", "icons");

    m_display.init(m_device);

    sfx::SoundManager::get().init();

    m_world.init(m_device, m_textureCache);
    m_world.setTerrainPreset(m_settings.terrainPreset);
    m_world.setRenderDistance(m_settings.renderDistanceChunks);
    m_camera.setFov(m_settings.fov);

    m_sky.init(m_device);
    m_outline.init(m_device, m_world);
    m_clouds.init(m_device);

    m_overlay.init(m_device, m_textureCache);

    m_gui.init(m_device, m_textureCache);
    m_gui.setQuitCallback([&] {
        m_running = false;
    });
    m_gui.setResumeCallback([&] {
        m_state = GameState::RUNNING;
    });

    m_gui.registerPauseSettingsActions({
        .toggleShadows = [&] { m_settings.shadows = !m_settings.shadows; },
        .toggleSsao = [&] {
            m_settings.ssao = !m_settings.ssao;
            m_display.setEffects(m_settings.ssao, m_settings.godRays);
        },
        .toggleGodRays = [&] {
            m_settings.godRays = !m_settings.godRays;
            m_display.setEffects(m_settings.ssao, m_settings.godRays);
        },
        .toggleWeather = [&] { m_settings.weather = !m_settings.weather; },
        .cycleTerrain = [&] {
            m_settings.terrainPreset = (m_settings.terrainPreset + 1) % 3;
            m_world.setTerrainPreset(m_settings.terrainPreset);
        },
        .fovDecrease = [&] {
            m_settings.fov = std::clamp(m_settings.fov - 5.0f, 45.0f, 110.0f);
            m_camera.setFov(m_settings.fov);
        },
        .fovIncrease = [&] {
            m_settings.fov = std::clamp(m_settings.fov + 5.0f, 45.0f, 110.0f);
            m_camera.setFov(m_settings.fov);
        },
        .timeSlower = [&] {
            m_settings.timeScale =
                std::clamp(m_settings.timeScale * 0.75f, 0.0f, 50.0f);
        },
        .timeFaster = [&] {
            m_settings.timeScale =
                std::clamp(m_settings.timeScale * 1.333333f, 0.0f, 50.0f);
        },
        .renderDistanceDecrease = [&] {
            m_settings.renderDistanceChunks =
                std::clamp(m_settings.renderDistanceChunks - 1, 2, 24);
            m_world.setRenderDistance(m_settings.renderDistanceChunks);
        },
        .renderDistanceIncrease = [&] {
            m_settings.renderDistanceChunks =
                std::clamp(m_settings.renderDistanceChunks + 1, 2, 24);
            m_world.setRenderDistance(m_settings.renderDistanceChunks);
        },
        .saveSettings = [&] { saveSettings(m_settings); },
    });

    m_gui.initPauseElements();

    m_playerEntity = m_ecs.creatEntity();
    auto transform = m_ecs.addComponent<cmp::Transform>(m_playerEntity);
    transform->position = glm::vec3(0.0f, 80.0f, 0.0f);

    m_ecs.addComponent<cmp::Velocity>(m_playerEntity);
    m_ecs.addComponent<cmp::Player>(m_playerEntity);

    auto *playerCollider = m_ecs.addComponent<cmp::Collider>(m_playerEntity);

    playerCollider->size = glm::vec3(0.6f, 1.8f, 0.6f);
    playerCollider->offset = glm::vec3(0.0f, 0.9f, 0.0f);
    playerCollider->groundOffset = 0.01f;
    playerCollider->isGhost = false;

    m_playerSystem.setInventoryOpenPtr(&m_inventoryOpen);
    m_playerSystem.setThirdPersonPtr(&m_thirdPerson);

    if (auto *play = m_ecs.getComponent<cmp::Player>(m_playerEntity)) {
        play->hotbar = {
            wld::BlockType::STONE,
            wld::BlockType::DIRT,
            wld::BlockType::COBBLESTONE,
            wld::BlockType::GRASS,
            wld::BlockType::LOG,
            wld::BlockType::SAND,
            wld::BlockType::LEAVES,
            wld::BlockType::FLOWER,
            wld::BlockType::ROSE,
        };
        play->backpack.fill(wld::BlockType::AIR);
        play->backpack[0] = wld::BlockType::STONE;
        play->backpack[1] = wld::BlockType::DIRT;
        play->backpack[2] = wld::BlockType::COBBLESTONE;
        play->backpack[3] = wld::BlockType::LOG;
        play->backpack[4] = wld::BlockType::SAND;
        play->backpack[5] = wld::BlockType::GRASS;
    }

    m_running = true;
}

void Game::destroy()
{
    m_device.waitIdle();

    saveSettings(m_settings);

    m_gui.destroy();

    m_overlay.destroy();

    m_clouds.destroy();
    m_outline.destroy();
    m_sky.destroy();
    m_world.destroy();

    sfx::SoundManager::get().destroy();

    m_display.destroy();
    m_textureCache.destroy();
    m_gpuData.destroy();
    m_device.destroy();

    m_window.destroy();
}

void Game::run()
{
    f64 lastTime = m_window.getCurrentTime();
    f64 accumulator = 0.0;
    
    int frameCount = 0;
    f64 fpsTimer = 0.0;
    f32 fps = 0.0;

    m_ecs.storePositions();
    
    while (m_running) {
        if (!m_window.isOpen()) {
            m_running = false;
        }

        f64 currentTime = m_window.getCurrentTime();
        f64 frameTime = currentTime - lastTime;
        lastTime = currentTime;

        frameCount++;
        fpsTimer += frameTime;
        if (fpsTimer >= 1.0) {
            fps = static_cast<f32>(frameCount) / static_cast<f32>(fpsTimer);
            frameCount = 0;
            fpsTimer = 0.0;
        }

        m_fps = fps;

        if (frameTime > 0.25) {
            frameTime = 0.25;
        }

        if (m_state == GameState::RUNNING) {
            accumulator += frameTime;
        }

        m_window.update();
        if (m_window.isMinimized()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            continue;
        }

        handleInput();

        while (accumulator >= MS_PER_TICK) {
            m_ecs.storePositions();
            tick(0.05f);
            accumulator -= MS_PER_TICK;
        }

        f32 alpha = static_cast<f32>(accumulator / MS_PER_TICK);

        update(alpha);
        
        render();
    }
}

void Game::handleInput()
{
    if (m_window.isKeyJustPressed(GLFW_KEY_ESCAPE)) {
        if (m_state == GameState::RUNNING && m_inventoryOpen) {
            m_inventoryOpen = false;
            m_invClickSlot = -1;
        } else if (m_state == GameState::RUNNING) {
            m_inventoryOpen = false;
            m_invClickSlot = -1;
            m_state = GameState::PAUSED;
        } else {
            m_state = GameState::RUNNING;
        }
    }

    if (m_state == GameState::RUNNING && m_window.isKeyJustPressed(GLFW_KEY_E)) {
        m_inventoryOpen = !m_inventoryOpen;
        if (!m_inventoryOpen) {
            m_invClickSlot = -1;
        }
    }

    if (m_state == GameState::RUNNING && m_window.isKeyJustPressed(GLFW_KEY_F5)
        && !m_inventoryOpen) {
        m_thirdPerson = !m_thirdPerson;
    }

    if (m_state == GameState::RUNNING && !m_inventoryOpen) {
        if (auto *pl = m_ecs.getComponent<cmp::Player>(m_playerEntity)) {
            for (int i = 0; i < 9; ++i) {
                if (m_window.isKeyJustPressed(GLFW_KEY_1 + i)) {
                    pl->hotbarSlot = i;
                }
            }
        }
    }

    handlePausedSettingsInput();

    if (m_state == GameState::RUNNING) {
        m_window.setCursorMode(
            m_inventoryOpen ? GLFW_CURSOR_NORMAL : GLFW_CURSOR_DISABLED
        );
    } else {
        m_window.setCursorMode(GLFW_CURSOR_NORMAL);
    }

    if (m_state == GameState::RUNNING && m_inventoryOpen
        && m_window.isMouseButtonJustPressed(GLFW_MOUSE_BUTTON_LEFT)) {
        handleInventoryClick(m_window.getMousePos());
    }

    if (m_state != GameState::RUNNING) {
        if (m_window.isMouseButtonJustPressed(GLFW_MOUSE_BUTTON_LEFT)) {
            m_gui.handleMouseClick();
        }
    }
}

void Game::update(f32 dt)
{
    syncSwapchainAndDisplay();

    /// Match the 3D pass aspect to the display framebuffer (often downscaled vs swapchain).
    const f32 fbW = static_cast<f32>(m_display.framebufferWidth());
    const f32 fbH = static_cast<f32>(m_display.framebufferHeight());
    f32 aspect =
        (fbW > 1e-4f && fbH > 1e-4f) ? (fbW / fbH) : m_window.getAspect();
    if (!std::isfinite(aspect) || aspect < 1e-4f) {
        aspect = 16.0f / 9.0f;
    }
    m_camera.updateProj(aspect);

    VkExtent2D extent = m_device.getExtent();
    m_camera.updateOrtho(extent.width, extent.height);

    constexpr f32 kPi = 3.14159265f;
    const f32 sunAngle = (m_dayPhase - 0.25f) * 2.0f * kPi;
    glm::vec3 sunDir = glm::normalize(glm::vec3(
        std::cos(sunAngle),
        std::sin(sunAngle),
        0.12f
    ));
    glm::mat4 lightMat = wld::World::computeLightMatrix(sunDir);

    m_gpuData.updateLight(lightMat, sunDir);
    m_gpuData.updateTime(m_window.getCurrentTime(), dt);

    updateGui();

    if (m_state != GameState::RUNNING) {
        m_camera.updateView();
        m_gpuData.updateCamera(m_camera);
        m_gpuData.update();

        m_display.setColor({0.6f, 0.6f, 0.6f, 1.0f});

        sfx::SoundManager::get().setListenerPos(
            m_camera.getPos(),
            m_camera.getFront(),
            m_camera.getUp()
        );
        sfx::SoundManager::get().update();
        return;
    }

    if (m_settings.weather) {
        m_display.setColor({0.88f, 0.9f, 0.96f, 1.0f});
    } else {
        m_display.setColor({1.0f, 1.0f, 1.0f, 1.0f});
    }

    m_display.setEffects(m_settings.ssao, m_settings.godRays);

    m_ecs.interpolate(dt);

    m_playerSystem.updateCamera();

    m_camera.updateView();
    m_gpuData.updateCamera(m_camera);
    m_gpuData.update();

    sfx::SoundManager::get().setListenerPos(
        m_camera.getPos(),
        m_camera.getFront(),
        m_camera.getUp()
    );
    sfx::SoundManager::get().update();
}

void Game::tick(f32 dt)
{
    if (m_state != GameState::RUNNING) {
        return;
    }

    const f32 dayTick = dt * (1.0f / 600.0f) * m_settings.timeScale;
    m_dayPhase += dayTick;
    while (m_dayPhase >= 1.0f) {
        m_dayPhase -= 1.0f;
    }

    m_world.update(m_camera.getPos(), dt);
    m_clouds.update(dt);

    m_playerSystem.tick(dt);
    m_physicsSystem.tick(dt);
}

void Game::render()
{
    auto cmd = m_device.beginFrame();
    if (!cmd) {
        return;
    }

    constexpr f32 kPi = 3.14159265f;
    const f32 sunAngle = (m_dayPhase - 0.25f) * 2.0f * kPi;
    glm::vec3 sunDir = glm::normalize(glm::vec3(
        std::cos(sunAngle),
        std::sin(sunAngle),
        0.12f
    ));
    glm::mat4 lightMat = wld::World::computeLightMatrix(sunDir);

    if (m_settings.shadows) {
        m_world.renderShadow(sunDir, cmd);
    }

    m_display.begin(cmd);

    const f32 weatherBlend = m_settings.weather ? 1.0f : 0.0f;
    m_sky.render(cmd, m_dayPhase, weatherBlend, sunDir);

    glm::vec4 sunPacked(sunDir, 0.0f);
    const u32 shadowTex = m_world.getShadowTextureID();
    m_world.render(
        m_camera,
        cmd,
        lightMat,
        sunPacked,
        shadowTex,
        m_settings.shadows
    );

    m_outline.render(cmd, m_camera);
    m_clouds.render(cmd, m_camera);
    m_overlay.render(cmd);

    m_display.end(cmd);

    glm::vec4 sunWorld = glm::vec4(m_camera.getPos() - sunDir * 1000.0f, 1.0f);
    glm::vec4 sunClip = m_camera.getProj() * m_camera.getView() * sunWorld;
    glm::vec2 sunScreen =
        glm::vec2(sunClip.x, sunClip.y) / sunClip.w * 0.5f + 0.5f;
    float clipStrength = glm::clamp(
        1.0f - glm::smoothstep(-0.2f, 0.2f, sunClip.z / sunClip.w),
        0.0f,
        1.0f
    );
    float elevStrength = glm::clamp((sunDir.y + 0.15f) / 1.15f, 0.0f, 1.0f);
    float sunStrength =
        m_settings.godRays ? glm::clamp(clipStrength * elevStrength, 0.0f, 1.0f) : 0.0f;
    m_display.setSun(sunScreen, sunStrength);

    m_device.beginRenderClear(cmd);
    m_display.draw(cmd);
    m_device.endRender(cmd);

    m_device.beginRenderLoad(cmd);
    m_gui.render(cmd);
    m_device.endRender(cmd);

    m_device.endFrame(cmd);
}

void Game::updateGui()
{
    gui::GameStat gameStat;
    gameStat.fps = static_cast<u32>(m_fps);
    gameStat.updatedChunks = m_world.getUpdatedChunks();
    gameStat.state = m_state;
    gameStat.settings = m_settings;
    gameStat.dayPhase01 = m_dayPhase;
    gameStat.inventoryOpen = m_inventoryOpen;
    if (auto *play = m_ecs.getComponent<cmp::Player>(m_playerEntity)) {
        gameStat.hotbar = play->hotbar;
        gameStat.hotbarSlot = play->hotbarSlot;
        gameStat.backpack = play->backpack;
    }

    m_gui.updateStat(gameStat);
    m_gui.update(m_window.getMousePos());
}

void Game::handleInventoryClick(const glm::vec2 &px)
{
    const int slot = m_gui.pickInventorySlot(px);
    if (slot < 0) {
        m_invClickSlot = -1;
        return;
    }

    auto *play = m_ecs.getComponent<cmp::Player>(m_playerEntity);
    if (!play) {
        return;
    }

    if (m_invClickSlot < 0) {
        m_invClickSlot = slot;
    } else if (m_invClickSlot == slot) {
        m_invClickSlot = -1;
    } else {
        wld::BlockType *a = unifiedInvSlot(play, m_invClickSlot);
        wld::BlockType *b = unifiedInvSlot(play, slot);
        if (a && b) {
            std::swap(*a, *b);
        }
        m_invClickSlot = -1;
    }
}

} // namespace game