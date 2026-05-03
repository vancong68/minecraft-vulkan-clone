#pragma once

#include "device.hpp"
#include "image.hpp"
#include "pipeline.hpp"
#include "framebuffer.hpp"

namespace gfx
{

class Display
{

public:
    Display() = default;
    ~Display() = default;

    void init(Device &device);
    void destroy();

    void resize(u32 width, u32 height);

    void begin(VkCommandBuffer cmd);
    void end(VkCommandBuffer cmd);

    void draw(VkCommandBuffer cmd);

public:
    void setColor(const glm::vec4 &color) { m_pc.color = color; }
    void setSun(const glm::vec2 &sunPos, float intensity) { m_pc.sun = glm::vec4(sunPos, intensity, 0.0f); }
    void setShadowMap(u32 shadowTextureID) { m_pc.shadowTextureID = shadowTextureID; }
    void setEffects(bool ssao, bool godRays)
    {
        m_pc.effects = (ssao ? 1u : 0u) | (godRays ? 2u : 0u);
    }

    u32 framebufferWidth() const { return m_framebuffer.getWidth(); }
    u32 framebufferHeight() const { return m_framebuffer.getHeight(); }

private:
    Device *m_device = nullptr;

    Framebuffer m_framebuffer;

    Pipeline m_pipeline;

    struct PushConstant
    {
        alignas(4) u32 textureID;
        alignas(4) u32 depthTextureID;
        alignas(4) u32 shadowTextureID;
        alignas(4) u32 padding;
        alignas(16) glm::vec4 color;
        alignas(16) glm::vec4 sun;
        alignas(4) u32 effects;
    };

    PushConstant m_pc;
};

} // namespace gfx