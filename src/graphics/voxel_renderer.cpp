#include "voxel_renderer.hpp"

#include <vector>

#include "world/block_registry.hpp"

namespace gfx
{

void VoxelRenderer::init(Device &device)
{
    m_device = &device;

    m_pipeline = Pipeline::Builder(device)
        .setShader("voxel.vert.spv", VK_SHADER_STAGE_VERTEX_BIT)
        .setShader("voxel.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
        .setPushConstant(sizeof(PushConstants))
        .setDepthTest(true)
        .setDepthWrite(true)
        .setCull(false)
        .build();

    // Build a small SSBO with block face UVs from blocks.toml.
    // Layout: for each blockId: 6 * u32 packed (x | (y << 16)) with (x,y) atlas tile coords.
    const auto &reg = wld::BlockRegistry::get();
    const u32 blockCount = reg.blockCount();

    std::vector<u32> uvTable;
    uvTable.resize(static_cast<size_t>(blockCount) * 6u, 0u);

    for (u32 id = 0; id < blockCount; ++id) {
        const auto &block = reg.getBlock(static_cast<int>(id));
        for (u32 f = 0; f < 6; ++f) {
            const glm::uvec2 uv = block.textures.faces[f];
            uvTable[static_cast<size_t>(id) * 6u + f] =
                (uv.x & 0xffffu) | ((uv.y & 0xffffu) << 16u);
        }
    }

    m_blockUvTable = device.createBuffer(
        sizeof(u32) * static_cast<VkDeviceSize>(uvTable.size()),
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );
    m_blockUvTable.uploadData(uvTable);
    m_blockUvTableSsboId = device.addSSBO(m_blockUvTable);
}

void VoxelRenderer::destroy()
{
    if (!m_device) {
        return;
    }

    if (m_blockUvTableSsboId != U32_MAX) {
        m_device->removeResource(m_blockUvTableSsboId);
        m_blockUvTableSsboId = U32_MAX;
    }
    m_blockUvTable.destroy();

    m_pipeline.destroy();
}

void VoxelRenderer::draw(
    VkCommandBuffer cmd,
    u32 chunkGridSsboId,
    u32 voxelAtlasSsboId,
    u32 terrainTextureId,
    const glm::vec3 &sunDir
)
{
    if (chunkGridSsboId == U32_MAX || voxelAtlasSsboId == U32_MAX) {
        return;
    }

    m_pipeline.bind(cmd);

    PushConstants pc{};
    pc.chunkGridSsboId = chunkGridSsboId;
    pc.voxelAtlasSsboId = voxelAtlasSsboId;
    pc.blockUvSsboId = m_blockUvTableSsboId;
    pc.terrainTextureId = terrainTextureId;
    pc.sunDir_ws = glm::vec4(glm::normalize(sunDir), 0.0f);

    m_pipeline.push(cmd, pc);

    // Fullscreen triangle.
    vkCmdDraw(cmd, 3, 1, 0, 0);
}

} // namespace gfx

