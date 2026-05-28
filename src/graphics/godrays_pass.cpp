#include "godrays_pass.hpp"

namespace gfx
{

void GodRaysPass::init(Device &device, u32 width, u32 height)
{
    m_device = &device;

    m_mask.init(device, width, height, VK_FORMAT_R8G8B8A8_UNORM, device.getDepthFormat(), false);
    m_rays.init(device, width, height, VK_FORMAT_R8G8B8A8_UNORM, device.getDepthFormat(), false);

    m_maskPipeline = Pipeline::Builder(device)
        .setShader("voxel.vert.spv", VK_SHADER_STAGE_VERTEX_BIT)
        .setShader("godrays_mask.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
        .setPushConstant(sizeof(MaskPC))
        .setDepthTest(false)
        .setDepthWrite(false)
        .setCull(false)
        .build();

    m_blurPipeline = Pipeline::Builder(device)
        .setShader("voxel.vert.spv", VK_SHADER_STAGE_VERTEX_BIT)
        .setShader("radial_blur.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
        .setPushConstant(sizeof(BlurPC))
        .setDepthTest(false)
        .setDepthWrite(false)
        .setCull(false)
        .build();
}

void GodRaysPass::destroy()
{
    m_mask.destroy();
    m_rays.destroy();
    m_maskPipeline.destroy();
    m_blurPipeline.destroy();
}

void GodRaysPass::resize(u32 width, u32 height)
{
    m_mask.resize(width, height);
    m_rays.resize(width, height);
}

void GodRaysPass::render(
    VkCommandBuffer cmd,
    u32 depthTextureId,
    const glm::vec2 &sunScreen01,
    float intensity
)
{
    if (depthTextureId == U32_MAX) {
        return;
    }

    // Mask: 1 where depth is far/sky, 0 where blocked.
    m_mask.begin(cmd);
    m_maskPipeline.bind(cmd);
    MaskPC mpc{};
    mpc.depthTextureId = depthTextureId;
    m_maskPipeline.push(cmd, mpc);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    m_mask.end(cmd);

    // Radial blur of the mask toward sun position.
    m_rays.begin(cmd);
    m_blurPipeline.bind(cmd);
    BlurPC bpc{};
    bpc.maskTextureId = m_mask.getTextureID();
    bpc.sunScreen01 = sunScreen01;
    bpc.intensity = intensity;
    m_blurPipeline.push(cmd, bpc);
    vkCmdDraw(cmd, 3, 1, 0, 0);
    m_rays.end(cmd);
}

} // namespace gfx

