#pragma once

#include "device.hpp"
#include "framebuffer.hpp"
#include "pipeline.hpp"

namespace gfx
{

class GodRaysPass
{
public:
    void init(Device &device, u32 width, u32 height);
    void destroy();
    void resize(u32 width, u32 height);

    void render(
        VkCommandBuffer cmd,
        u32 depthTextureId,
        const glm::vec2 &sunScreen01,
        float intensity
    );

    u32 getRaysTextureId() const { return m_rays.getTextureID(); }

private:
    Device *m_device = nullptr;

    Framebuffer m_mask;
    Framebuffer m_rays;

    Pipeline m_maskPipeline;
    Pipeline m_blurPipeline;

    struct MaskPC
    {
        alignas(4) u32 depthTextureId;
        alignas(4) u32 _pad0;
        alignas(4) u32 _pad1;
        alignas(4) u32 _pad2;
    };

    struct BlurPC
    {
        alignas(4) u32 maskTextureId;
        alignas(4) u32 _pad0;
        alignas(8) glm::vec2 sunScreen01;
        alignas(4) float intensity;
        alignas(4) u32 _pad1;
    };
};

} // namespace gfx

