#pragma once

#include "device.hpp"
#include "pipeline.hpp"
#include "buffer.hpp"

#include <glm/glm.hpp>

namespace gfx
{

class VoxelRenderer
{
public:
    void init(Device &device);
    void destroy();

    void draw(
        VkCommandBuffer cmd,
        u32 chunkGridSsboId,
        u32 voxelAtlasSsboId,
        u32 terrainTextureId,
        u32 aoTextureId,
        const glm::vec3 &sunDir
    );

private:
    Device *m_device = nullptr;
    Pipeline m_pipeline;

    Buffer m_blockUvTable;
    u32 m_blockUvTableSsboId = U32_MAX;

    struct PushConstants
    {
        alignas(4) u32 chunkGridSsboId;
        alignas(4) u32 voxelAtlasSsboId;
        alignas(4) u32 blockUvSsboId;
        alignas(4) u32 terrainTextureId;
        alignas(4) u32 aoTextureId;
        alignas(16) glm::vec4 sunDir_ws;
    };
};

} // namespace gfx

