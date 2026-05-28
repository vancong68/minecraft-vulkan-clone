#pragma once

#include "device.hpp"
#include "framebuffer.hpp"
#include "pipeline.hpp"

namespace gfx
{

class SsaoPass
{
public:
    void init(Device &device, u32 width, u32 height);
    void destroy();
    void resize(u32 width, u32 height);

    void render(VkCommandBuffer cmd, u32 depthTextureId);

    u32 getAoTextureId() const { return m_fb.getTextureID(); }

private:
    Device *m_device = nullptr;
    Framebuffer m_fb;
    Pipeline m_pipeline;

    struct PushConstants
    {
        alignas(4) u32 depthTextureId;
        alignas(4) u32 _pad0;
        alignas(4) u32 _pad1;
        alignas(4) u32 _pad2;
    };
};

} // namespace gfx

