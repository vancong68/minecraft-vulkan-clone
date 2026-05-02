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

private:
    Device *m_device = nullptr;

    Framebuffer m_framebuffer;

    Pipeline m_pipeline;

    struct PushConstant
    {
        alignas(4) u32 textureID;
        alignas(16) glm::vec4 color;
        alignas(16) glm::vec4 sun;
    };

    PushConstant m_pc;
};

} // namespace gfx