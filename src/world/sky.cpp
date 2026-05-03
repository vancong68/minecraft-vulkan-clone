#include "sky.hpp"

namespace wld
{

namespace
{

struct SkyPushConstants
{
    glm::vec4 params;
    glm::vec4 sunDirWorld;
};

} // namespace

void Sky::init(gfx::Device &device)
{
    UNUSED(device);

    m_pipeline = gfx::Pipeline::Builder(device)
        .setShader("sky.vert.spv", VK_SHADER_STAGE_VERTEX_BIT)
        .setShader("sky.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
        .setPushConstant(sizeof(SkyPushConstants))
        .setDepthTest(false)
        .setDepthWrite(false)
        .setCullMode(VK_CULL_MODE_NONE)
        .build();
}

void Sky::destroy()
{
    m_pipeline.destroy();
}

void Sky::render(
    VkCommandBuffer cmd,
    f32 dayPhase01,
    f32 weather01,
    const glm::vec3 &sunDirectionWorld
)
{
    SkyPushConstants pc{};
    pc.params = glm::vec4(
        glm::clamp(dayPhase01, 0.0f, 1.0f),
        glm::clamp(weather01, 0.0f, 1.0f),
        0.0f,
        0.0f
    );
    glm::vec3 s = sunDirectionWorld;
    if (glm::length(s) > 1e-5f) {
        s = glm::normalize(s);
    } else {
        s = glm::vec3(0.0f, 1.0f, 0.0f);
    }
    pc.sunDirWorld = glm::vec4(s, 0.0f);

    m_pipeline.bind(cmd);
    m_pipeline.push(cmd, pc);
    vkCmdDraw(cmd, 36, 1, 0, 0);
}

} // namespace wld
