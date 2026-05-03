#pragma once

#include <cstddef>

#include <vector>
#include <unordered_map>
#include <memory>
#include <unordered_set>
#include <algorithm>
#include <thread>
#include <mutex>
#include <queue>
#include <future>

#include "chunk.hpp"
#include "chunk_mesh.hpp"
#include "block.hpp"
#include "block_registry.hpp"
#include "world_generator.hpp"
#include "core/camera/camera.hpp"
#include "graphics/device.hpp"
#include "graphics/pipeline.hpp"
#include "graphics/texture_cache.hpp"
#include "core/frustum.hpp"

namespace wld
{

struct Ray
{
    glm::vec3 origin;
    glm::vec3 direction;
};

struct RaycastResult
{
    glm::ivec3 pos;
    glm::ivec3 normal;
    Face face;
};

class World
{

public:
    void init(gfx::Device &device, gfx::TextureCache &textureCache);
    void destroy();

    void update(const glm::vec3 &playerPos, f32 dt);
    void render(
        const core::Camera &camera,
        VkCommandBuffer cmd,
        const glm::mat4 &lightMatrix,
        const glm::vec4 &sunDirPacked,
        u32 shadowMapTextureID,
        bool shadowsEnabled
    );
    void renderShadow(const glm::vec3 &sunDir, VkCommandBuffer cmd);

    static glm::mat4 computeLightMatrix(const glm::vec3 &sunDir);

    u32 getShadowTextureID() const { return m_shadowTextureID; }

    void setTerrainPreset(int preset);
    void setRenderDistance(int chunkRadius);
    int getRenderDistance() const { return m_renderDistance; }

    BlockType getBlock(int x, int y, int z) const;
    BlockType getBlock(const glm::ivec3 &pos) const {
        return getBlock(pos.x, pos.y, pos.z);
    }
    
    void placeBlock(const glm::ivec3 &pos, BlockType type);
    void deleteBlock(const glm::ivec3 &pos);

    bool raycast(const Ray &ray, f32 maxDistance, RaycastResult &result);
    bool checkCollision(const glm::vec3 &min, const glm::vec3 &max);

    usize getUpdatedChunks() const { return m_updatedChunks; }

public:
    Chunk *getChunk(const ChunkPos &pos) const;


private:
    struct ChunkPosHash
    {
        std::size_t operator()(const ChunkPos &pos) const {
            return std::hash<int>()(pos.x) ^ (std::hash<int>()(pos.z) << 1);
        }
    };
    
    std::unordered_set<ChunkPos, ChunkPosHash> m_chunksNeeded;
    std::vector<std::pair<ChunkPos, f32>> m_chunksToLoad;
    std::vector<ChunkPos> m_chunksToUnload;

    void loadChunks(const ChunkPos &pos);
    void unloadChunks(const ChunkPos &pos);
    bool isChunkLoaded(const ChunkPos &pos);

    void updateMeshe(const ChunkPos &pos);

    static constexpr int CHUNKS_PER_TICK = 1;

    int m_renderDistance = 8;

    std::queue<ChunkPos> m_pendingChunks;
    std::queue<ChunkPos> m_pendingMeshes;

    usize m_updatedChunks = 0;

    gfx::Device *m_device;

    enum PipelineType
    {
        P_OPAQUE,
        P_TRANSPARENT,
        P_CROSS,
    };

    std::array<gfx::Pipeline, 3> m_pipelines;
    gfx::Pipeline m_shadowPipeline;

    u32 m_textureID;
    u32 m_shadowTextureID = U32_MAX;

    gfx::Image m_shadowImage;

    struct ShadowPushConstants
    {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 shadowMatrix;
        alignas(4) u32 textureID;
    };

    struct ChunkPushConstants
    {
        alignas(16) glm::mat4 model;
        alignas(16) glm::mat4 shadowMatrix;
        alignas(16) glm::vec4 sunDir;
        /// World-space camera position for fog/distance (avoids flaky UBO reads on some GPUs).
        alignas(16) glm::vec4 camWorldPos;
        alignas(4) u32 textureID;
        alignas(4) u32 shadowMapTextureID;
    };

    static_assert(sizeof(ChunkPushConstants) == 176);
    static_assert(offsetof(ChunkPushConstants, textureID) == 160);

    core::Frustum m_frustum;

    using ChunkMap = std::unordered_map<ChunkPos,
        std::unique_ptr<Chunk>, 
        ChunkPosHash>;
    using ChunkMeshMap = std::unordered_map<ChunkPos, 
        std::unique_ptr<ChunkMesh>, 
        ChunkPosHash>;

    ChunkPos m_playerChunkPos;
    ChunkMap m_chunks;
    ChunkMeshMap m_meshes;

    WorldGenerator m_generator;
};

} // namespace wld