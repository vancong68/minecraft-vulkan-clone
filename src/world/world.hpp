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

#include "chunk.hpp"
#include "block.hpp"
#include "block_registry.hpp"
#include "world_generator.hpp"
#include "core/thread_pool.hpp"
#include "core/camera/camera.hpp"
#include "graphics/texture_cache.hpp"
#include "graphics/buffer.hpp"
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

    static glm::mat4 computeLightMatrix(const glm::vec3 &sunDir);

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

    u32 getGpuChunkGridSsboId() const { return m_gpuChunkGridSsboId; }
    u32 getGpuVoxelAtlasSsboId() const { return m_gpuVoxelAtlasSsboId; }

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

    usize m_updatedChunks = 0;

    gfx::Device *m_device;

    core::Frustum m_frustum;

    using ChunkMap = std::unordered_map<ChunkPos,
        std::unique_ptr<Chunk>, 
        ChunkPosHash>;

    ChunkPos m_playerChunkPos;
    ChunkMap m_chunks;

    WorldGenerator m_generator;

private:
    // --- Chunk generation threading ---
    core::ThreadPool m_chunkThreadPool;
    std::mutex m_completedChunksMutex;
    std::queue<std::unique_ptr<Chunk>> m_completedChunks;
    std::unordered_set<ChunkPos, ChunkPosHash> m_inFlightChunks;

    void startChunkThreads();
    void stopChunkThreads();
    void enqueueChunkGen(const ChunkPos &pos);
    void pollCompletedChunkGen();

private:
    // --- GPU voxel data (used by fullscreen voxel raycaster) ---
    struct GpuChunkGridHeader
    {
        alignas(4) i32 originChunkX = 0;
        alignas(4) i32 originChunkZ = 0;
        alignas(4) u32 gridSize = 0;      // (2*renderDistance + 1)
        alignas(4) u32 _pad0 = 0;
    };

    static constexpr u32 kInvalidChunkSlot = 0xffffffffu;

    // Storage buffer layout:
    // [GpuChunkGridHeader][u32 chunkSlotIndex[gridSize*gridSize]]
    gfx::Buffer m_gpuChunkGrid;
    u32 m_gpuChunkGridSsboId = U32_MAX;

    // Storage buffer layout: u32 voxels[maxChunkSlots * Chunk::VOXELS_PER_CHUNK]
    // voxel encoding: (blockId & 0xff) | ((light & 0xff) << 8)
    gfx::Buffer m_gpuVoxelAtlas;
    u32 m_gpuVoxelAtlasSsboId = U32_MAX;

    u32 m_gpuChunkGridSize = 0;
    u32 m_gpuMaxChunkSlots = 0;
    std::vector<u32> m_gpuFreeChunkSlots;
    std::unordered_map<ChunkPos, u32, ChunkPosHash> m_gpuChunkSlotByPos;

    void initGpuVoxelData();
    void destroyGpuVoxelData();
    void rebuildGpuChunkGridHeaderAndClear();
    void uploadChunkVoxelsToGpu(const Chunk &chunk);
    void setGpuChunkGridSlot(const ChunkPos &pos, u32 slotIndex);
    u32 allocateGpuChunkSlot(const ChunkPos &pos);
    void freeGpuChunkSlot(const ChunkPos &pos);
};

} // namespace wld