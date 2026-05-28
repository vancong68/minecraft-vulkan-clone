#include "ssao_pass.hpp"

namespace gfx
{

void SsaoPass::init(Device &device, u32 width, u32 height)
{
    m_device = &device;

    m_fb.init(device, width, height, VK_FORMAT_R8G8B8A8_UNORM, device.getDepthFormat(), false);

    m_pipeline = Pipeline::Builder(device)
        .setShader("voxel.vert.spv", VK_SHADER_STAGE_VERTEX_BIT) // reuse fullscreen triangle
        .setShader("ssao.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
        .setPushConstant(sizeof(PushConstants))
        .setDepthTest(false)
        .setDepthWrite(false)
        .setCull(false)
        .build();
}

void SsaoPass::destroy()
{
    m_fb.destroy();
    m_pipeline.destroy();
}

void SsaoPass::resize(u32 width, u32 height)
{
    m_fb.resize(width, height);
}

void SsaoPass::render(VkCommandBuffer cmd, u32 depthTextureId)
{
    if (depthTextureId == U32_MAX) {
        return;
    }

    m_fb.begin(cmd);
    m_pipeline.bind(cmd);

    PushConstants pc{};
    pc.depthTextureId = depthTextureId;
    m_pipeline.push(cmd, pc);

    vkCmdDraw(cmd, 3, 1, 0, 0);
    m_fb.end(cmd);
}

} // namespace gfx

