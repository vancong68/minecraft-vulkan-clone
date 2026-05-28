#include "world.hpp"

#include "core/debug_log.hpp"
#include <algorithm>
#include <cstring>
#include <glm/gtc/matrix_transform.hpp>

namespace wld
{

glm::mat4 World::computeLightMatrix(const glm::vec3 &sunDir)
{
    glm::mat4 lightProjection = glm::ortho(-100.0f, 100.0f, -100.0f, 100.0f, -200.0f, 200.0f);
    glm::mat4 lightView = glm::lookAt(
        sunDir * 100.0f,
        glm::vec3(0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );
    return lightProjection * lightView;
}

void World::setTerrainPreset(int preset)
{
    m_generator.configureTerrainPreset(preset);
}

void World::setRenderDistance(int chunkRadius)
{
    int c = std::clamp(chunkRadius, 2, 24);
    if (c == m_renderDistance) {
        return;
    }
    m_renderDistance = c;
    m_playerChunkPos = {0x3fffffff, 0x3fffffff};
}

void World::init(gfx::Device &device, gfx::TextureCache &textureCache)
{
    m_device = &device;

    m_playerChunkPos = {-1, -1};
    UNUSED(textureCache);

    m_chunks.reserve(static_cast<size_t>(m_renderDistance) * static_cast<size_t>(m_renderDistance) * 4);

    m_generator.init(0);

    startChunkThreads();
    initGpuVoxelData();
}

void World::destroy()
{
    destroyGpuVoxelData();
    stopChunkThreads();

    m_chunks.clear();
}

void World::update(const glm::vec3 &playerPos, f32 dt)
{
    pollCompletedChunkGen();

    ChunkPos newPos = {
        static_cast<i32>(playerPos.x) / Chunk::CHUNK_SIZE,
        static_cast<i32>(playerPos.z) / Chunk::CHUNK_SIZE
    };

    static f32 time = 0.0f;
    time += dt;

    if (time >= 1.0f) {
        time = 0.0f;
        m_updatedChunks = m_pendingChunks.size();
    }
    
    const int rd = m_renderDistance;
    const f32 squaredDist = static_cast<f32>(rd * rd);

    m_chunksNeeded.clear();
    m_chunksToLoad.clear();
    m_chunksToUnload.clear();

    if (newPos != m_playerChunkPos || m_pendingChunks.empty()) {
        std::queue<ChunkPos> empty;
        std::swap(m_pendingChunks, empty);

        for (int x = -rd; x <= rd; x++) {
            for (int z = -rd; z <= rd; z++) {
                if (x * x + z * z > squaredDist) continue;

                ChunkPos pos = {newPos.x + x, newPos.z + z};

                m_chunksNeeded.insert(pos);

                if (!isChunkLoaded(pos)) {
                    f32 maxDist = static_cast<f32>(x * x + z * z);
                    m_chunksToLoad.push_back({pos, maxDist});
                }
            }
        }

        std::sort(
            m_chunksToLoad.begin(),
            m_chunksToLoad.end(),
            [](const auto &a, const auto &b) {
                return a.second < b.second;
            }
        );

        for (const auto &[pos, dist] : m_chunksToLoad) {
            m_pendingChunks.push(pos);
        }

        for (const auto &[pos, chunk] : m_chunks) {
            if (m_chunksNeeded.find(pos) == m_chunksNeeded.end()) {
                m_chunksToUnload.push_back(pos);
            }
        }

        for (const auto &pos : m_chunksToUnload) {
            unloadChunks(pos);
        }

        m_playerChunkPos = newPos;

        rebuildGpuChunkGridHeaderAndClear();
        for (const auto &[pos, chunk] : m_chunks) {
            if (auto it = m_gpuChunkSlotByPos.find(pos); it != m_gpuChunkSlotByPos.end()) {
                setGpuChunkGridSlot(pos, it->second);
            }
        }
    }

    int chunksLoaded = 0;
    while (!m_pendingChunks.empty() && chunksLoaded < CHUNKS_PER_TICK) {
        ChunkPos pos = m_pendingChunks.front();
        m_pendingChunks.pop();

        if (
            !isChunkLoaded(pos) &&
            m_chunksNeeded.find(pos) != m_chunksNeeded.end()
        ) {
            enqueueChunkGen(pos);
            chunksLoaded++;
        }
    }
}

BlockType World::getBlock(int x, int y, int z) const
{
    if (y < 0 || y >= Chunk::CHUNK_HEIGHT) {
        return BlockType::AIR;
    }

    ChunkPos chunkPos(
        (x < 0) ?(x - (Chunk::CHUNK_SIZE - 1)) / Chunk::CHUNK_SIZE :
            x / Chunk::CHUNK_SIZE,
        (z < 0) ? (z - (Chunk::CHUNK_SIZE - 1)) / Chunk::CHUNK_SIZE :
            z / Chunk::CHUNK_SIZE
    );

    auto it = m_chunks.find(chunkPos);
    if (it == m_chunks.end()) {
        return BlockType::AIR;
    }

    int localX = x - (chunkPos.x * Chunk::CHUNK_SIZE);
    int localZ = z - (chunkPos.z * Chunk::CHUNK_SIZE);

    return it->second->getBlock(localX, y, localZ);
}

void World::placeBlock(const glm::ivec3 &pos, BlockType type)
{
    ChunkPos chunkPos = {
        (pos.x < 0) ? (pos.x - (Chunk::CHUNK_SIZE - 1)) / Chunk::CHUNK_SIZE : pos.x / Chunk::CHUNK_SIZE,
        (pos.z < 0) ? (pos.z - (Chunk::CHUNK_SIZE - 1)) / Chunk::CHUNK_SIZE : pos.z / Chunk::CHUNK_SIZE
    };

    if (auto it = m_chunks.find(chunkPos); it != m_chunks.end()) {
        glm::ivec3 localPos = {
            pos.x - (chunkPos.x * Chunk::CHUNK_SIZE),
            pos.y,
            pos.z - (chunkPos.z * Chunk::CHUNK_SIZE)
        };

        it->second->setBlock(localPos, type);

        it->second->update();

        uploadChunkVoxelsToGpu(*it->second);
    }
}

void World::deleteBlock(const glm::ivec3 &pos)
{
    placeBlock(pos, BlockType::AIR);
}

bool World::raycast(
    const Ray &ray,
    f32 maxDistance,
    RaycastResult &result
)
{
    glm::vec3 pos = ray.origin;
    glm::vec3 step = glm::sign(ray.direction);
    glm::vec3 tDelta = glm::abs(1.0f / ray.direction);
    glm::vec3 tMax;
    glm::ivec3 blockPos = glm::floor(pos);

    for (i32 i = 0; i < 3; i++) {
        if (step[i] > 0) {
            tMax[i] = ((blockPos[i] + 1) - pos[i]) * tDelta[i];
        } else {
            tMax[i] = (pos[i] - blockPos[i]) * tDelta[i];
        }
    }

    Face hitFace;
    f32 dist = 0.0f;

    while (dist < maxDistance) {
        if (tMax.x < tMax.y && tMax.x < tMax.z) {
            blockPos.x += step.x;
            dist = tMax.x;
            tMax.x += tDelta.x;
            hitFace = (step.x > 0) ? Face::WEST : Face::EAST;
        } else if (tMax.y < tMax.z) {
            blockPos.y += step.y;
            dist = tMax.y;
            tMax.y += tDelta.y;
            hitFace = (step.y > 0) ? Face::BOTTOM : Face::TOP;
        } else {
            blockPos.z += step.z;
            dist = tMax.z;
            tMax.z += tDelta.z;
            hitFace = (step.z > 0) ? Face::NORTH : Face::SOUTH;
        }

        BlockType type = getBlock(blockPos);
        if (
            type != BlockType::AIR &&
            wld::BlockRegistry::get().getBlock(type).breakable
        ) {
            result.pos = blockPos;
            result.face = hitFace;

            result.normal = blockPos;
            switch (hitFace) {
            case Face::NORTH:
                result.normal.z--;
                break;
            case Face::SOUTH:
                result.normal.z++;
                break;
            case Face::EAST:
                result.normal.x++;
                break;
            case Face::WEST:
                result.normal.x--;
                break;
            case Face::TOP:
                result.normal.y++;
                break;
            case Face::BOTTOM:
                result.normal.y--;
                break;
            }

            return true;
        }
    }

    return false;
}

bool World::checkCollision(const glm::vec3 &min, const glm::vec3 &max)
{
    i32 minX = static_cast<i32>(std::floor(min.x));
    i32 minY = static_cast<i32>(std::floor(min.y));
    i32 minZ = static_cast<i32>(std::floor(min.z));
    i32 maxX = static_cast<i32>(std::floor(max.x));
    i32 maxY = static_cast<i32>(std::floor(max.y));
    i32 maxZ = static_cast<i32>(std::floor(max.z));

    minY = std::max(minY, 0);
    maxY = std::min(maxY, Chunk::CHUNK_HEIGHT - 1);

    ChunkPos currentChunk = {INT_MAX, INT_MAX};
    const Chunk *chunk = nullptr;

    for (int x = minX; x <= maxX; ++x) {
        for (int z = minZ; z <= maxZ; ++z) {
            ChunkPos chunkPos = {
                (x < 0) ? (x - (Chunk::CHUNK_SIZE - 1)) / Chunk::CHUNK_SIZE :
                    x / Chunk::CHUNK_SIZE,
                (z < 0) ? (z - (Chunk::CHUNK_SIZE - 1)) / Chunk::CHUNK_SIZE :
                    z / Chunk::CHUNK_SIZE
            };

            if (chunkPos.x != currentChunk.x || chunkPos.z != currentChunk.z) {
                currentChunk = chunkPos;
                chunk = getChunk(chunkPos);

                if (!chunk) { continue; }
            }

            i32 localX = x - (chunkPos.x * Chunk::CHUNK_SIZE);
            i32 localZ = z - (chunkPos.z * Chunk::CHUNK_SIZE);

            for (i32 y = minY; y <= maxY; ++y) {
                BlockType block = chunk->getBlock(localX, y, localZ);

                if (
                    chunk &&
                    block != BlockType::AIR &&
                    wld::BlockRegistry::get().getBlock(block).collision
                ) {
                    return true;
                }
            }
        }
    }

    return false;
}

Chunk *World::getChunk(const ChunkPos &pos) const
{
    if (auto it = m_chunks.find(pos); it != m_chunks.end()) {
        return it->second.get();
    }

    return nullptr;
}

void World::loadChunks(const ChunkPos &pos)
{
    auto chunk = std::make_unique<Chunk>(*this, pos);

    m_generator.generateChunk(*chunk, pos);

    chunk->update();

    m_chunks[pos] = std::move(chunk);

    if (auto *c = getChunk(pos)) {
        uploadChunkVoxelsToGpu(*c);
    }
}

void World::startChunkThreads()
{
    const unsigned int hc = std::thread::hardware_concurrency();
    const std::size_t threads =
        static_cast<std::size_t>((hc > 1) ? (hc - 1) : 1);
    m_chunkThreadPool.start(threads);
}

void World::stopChunkThreads()
{
    m_chunkThreadPool.stop();
    {
        std::lock_guard<std::mutex> lock(m_completedChunksMutex);
        std::queue<std::unique_ptr<Chunk>> empty;
        std::swap(m_completedChunks, empty);
    }
    m_inFlightChunks.clear();
}

void World::enqueueChunkGen(const ChunkPos &pos)
{
    if (isChunkLoaded(pos)) {
        return;
    }
    if (m_inFlightChunks.find(pos) != m_inFlightChunks.end()) {
        return;
    }

    m_inFlightChunks.insert(pos);

    // NOTE: Chunk generation is CPU-only; GPU upload is deferred to main thread.
    m_chunkThreadPool.enqueue([this, pos] {
        auto chunk = std::make_unique<Chunk>(*this, pos);
        m_generator.generateChunk(*chunk, pos);
        chunk->update();

        std::lock_guard<std::mutex> lock(m_completedChunksMutex);
        m_completedChunks.push(std::move(chunk));
    });
}

void World::pollCompletedChunkGen()
{
    static int s_pollLogCount = 0;
    bool gpuWrites = false;
    // Drain completed chunks quickly on main thread.
    for (;;) {
        std::unique_ptr<Chunk> chunk;
        {
            std::lock_guard<std::mutex> lock(m_completedChunksMutex);
            if (m_completedChunks.empty()) {
                break;
            }
            chunk = std::move(m_completedChunks.front());
            m_completedChunks.pop();
        }
        if (!chunk) {
            continue;
        }

        const ChunkPos pos = chunk->pos();
        m_inFlightChunks.erase(pos);

        // If player moved and this chunk is no longer needed, drop it.
        // (If m_chunksNeeded isn't computed yet for this frame, accept and let the normal unload path handle it.)
        if (!m_chunksNeeded.empty() && m_chunksNeeded.find(pos) == m_chunksNeeded.end()) {
            continue;
        }

        // Insert chunk and upload to GPU voxel atlas.
        m_chunks[pos] = std::move(chunk);

        if (auto *c = getChunk(pos)) {
            if (!gpuWrites) {
                m_device->waitIdle();
                gpuWrites = true;
            }
            uploadChunkVoxelsToGpu(*c);
        }

        if (s_pollLogCount < 8) {
            core::debugLog(
                "C",
                "world.cpp:pollCompletedChunkGen",
                "chunk_integrated",
                "{\"cx\":" + std::to_string(pos.x) + ",\"cz\":" + std::to_string(pos.z)
                    + ",\"loaded\":" + std::to_string(m_chunks.size()) + "}"
            );
            ++s_pollLogCount;
        }
    }
}

void World::unloadChunks(const ChunkPos &pos)
{
    freeGpuChunkSlot(pos);
    m_chunks.erase(pos);
}

bool World::isChunkLoaded(const ChunkPos &pos)
{
    return m_chunks.find(pos) != m_chunks.end();
}

void World::initGpuVoxelData()
{
    destroyGpuVoxelData();

    const u32 gridSize = static_cast<u32>(2 * m_renderDistance + 1);
    m_gpuChunkGridSize = gridSize;

    // Allocate enough slots for the full grid. (We can optimize later to circle-only.)
    m_gpuMaxChunkSlots = gridSize * gridSize;
    if (m_gpuMaxChunkSlots == 0) {
        return;
    }

    const VkDeviceSize gridBytes =
        sizeof(GpuChunkGridHeader) + sizeof(u32) * static_cast<VkDeviceSize>(m_gpuMaxChunkSlots);
    m_gpuChunkGrid = m_device->createBuffer(
        gridBytes,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );

    const VkDeviceSize atlasBytes =
        sizeof(u32) * static_cast<VkDeviceSize>(Chunk::VOXELS_PER_CHUNK) *
        static_cast<VkDeviceSize>(m_gpuMaxChunkSlots);
    m_gpuVoxelAtlas = m_device->createBuffer(
        atlasBytes,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
        VMA_MEMORY_USAGE_CPU_TO_GPU
    );

    m_gpuChunkGridSsboId = m_device->addSSBO(m_gpuChunkGrid);
    m_gpuVoxelAtlasSsboId = m_device->addSSBO(m_gpuVoxelAtlas);

    m_gpuFreeChunkSlots.clear();
    m_gpuFreeChunkSlots.reserve(m_gpuMaxChunkSlots);
    for (u32 i = 0; i < m_gpuMaxChunkSlots; ++i) {
        m_gpuFreeChunkSlots.push_back(m_gpuMaxChunkSlots - 1 - i);
    }
    m_gpuChunkSlotByPos.clear();

    rebuildGpuChunkGridHeaderAndClear();
}

void World::destroyGpuVoxelData()
{
    if (m_device) {
        if (m_gpuChunkGridSsboId != U32_MAX) {
            m_device->removeResource(m_gpuChunkGridSsboId);
            m_gpuChunkGridSsboId = U32_MAX;
        }
        if (m_gpuVoxelAtlasSsboId != U32_MAX) {
            m_device->removeResource(m_gpuVoxelAtlasSsboId);
            m_gpuVoxelAtlasSsboId = U32_MAX;
        }
    }

    m_gpuChunkGrid.destroy();
    m_gpuVoxelAtlas.destroy();

    m_gpuChunkGridSize = 0;
    m_gpuMaxChunkSlots = 0;
    m_gpuFreeChunkSlots.clear();
    m_gpuChunkSlotByPos.clear();
}

void World::rebuildGpuChunkGridHeaderAndClear()
{
    if (!m_gpuChunkGrid.isValid() || m_gpuMaxChunkSlots == 0) {
        return;
    }

    // SSBO is sampled by the voxel raycast shader; do not map while GPU may still be reading it.
    m_device->waitIdle();

    const i32 originX = m_playerChunkPos.x - m_renderDistance;
    const i32 originZ = m_playerChunkPos.z - m_renderDistance;

    auto *mapped = static_cast<u8 *>(m_gpuChunkGrid.map());
    if (!mapped) {
        return;
    }

    GpuChunkGridHeader header{};
    header.originChunkX = originX;
    header.originChunkZ = originZ;
    header.gridSize = m_gpuChunkGridSize;
    std::memcpy(mapped, &header, sizeof(header));

    auto *slots = reinterpret_cast<u32 *>(mapped + sizeof(GpuChunkGridHeader));
    for (u32 i = 0; i < m_gpuMaxChunkSlots; ++i) {
        slots[i] = kInvalidChunkSlot;
    }

    const VkDeviceSize gridBytes =
        sizeof(GpuChunkGridHeader) + sizeof(u32) * static_cast<VkDeviceSize>(m_gpuMaxChunkSlots);
    m_gpuChunkGrid.flushMappedRange(0, gridBytes);
    m_gpuChunkGrid.unmap();
}

u32 World::allocateGpuChunkSlot(const ChunkPos &pos)
{
    if (auto it = m_gpuChunkSlotByPos.find(pos); it != m_gpuChunkSlotByPos.end()) {
        return it->second;
    }
    if (m_gpuFreeChunkSlots.empty()) {
        return kInvalidChunkSlot;
    }

    u32 slot = m_gpuFreeChunkSlots.back();
    m_gpuFreeChunkSlots.pop_back();
    m_gpuChunkSlotByPos[pos] = slot;
    return slot;
}

void World::freeGpuChunkSlot(const ChunkPos &pos)
{
    auto it = m_gpuChunkSlotByPos.find(pos);
    if (it == m_gpuChunkSlotByPos.end()) {
        return;
    }

    const u32 slot = it->second;
    m_gpuChunkSlotByPos.erase(it);
    m_gpuFreeChunkSlots.push_back(slot);

    setGpuChunkGridSlot(pos, kInvalidChunkSlot);
}

void World::setGpuChunkGridSlot(const ChunkPos &pos, u32 slotIndex)
{
    if (!m_gpuChunkGrid.isValid() || m_gpuChunkGridSize == 0) {
        return;
    }

    const i32 localX = pos.x - (m_playerChunkPos.x - m_renderDistance);
    const i32 localZ = pos.z - (m_playerChunkPos.z - m_renderDistance);
    if (localX < 0 || localZ < 0) {
        return;
    }

    const u32 ux = static_cast<u32>(localX);
    const u32 uz = static_cast<u32>(localZ);
    if (ux >= m_gpuChunkGridSize || uz >= m_gpuChunkGridSize) {
        return;
    }

    const u32 idx = uz * m_gpuChunkGridSize + ux;

    auto *mapped = static_cast<u8 *>(m_gpuChunkGrid.map());
    if (!mapped) {
        return;
    }

    auto *slots = reinterpret_cast<u32 *>(mapped + sizeof(GpuChunkGridHeader));
    slots[idx] = slotIndex;
    const VkDeviceSize slotByteOff =
        sizeof(GpuChunkGridHeader) + static_cast<VkDeviceSize>(idx) * sizeof(u32);
    m_gpuChunkGrid.flushMappedRange(slotByteOff, sizeof(u32));
    m_gpuChunkGrid.unmap();
}

void World::uploadChunkVoxelsToGpu(const Chunk &chunk)
{
    if (!m_gpuVoxelAtlas.isValid() || !m_device) {
        return;
    }

    const ChunkPos pos = chunk.pos();
    const u32 slotIndex = allocateGpuChunkSlot(pos);
    if (slotIndex == kInvalidChunkSlot) {
        return;
    }

    setGpuChunkGridSlot(pos, slotIndex);

    static int s_uploadLogCount = 0;
    if (s_uploadLogCount < 8) {
        const i32 localX = pos.x - (m_playerChunkPos.x - m_renderDistance);
        const i32 localZ = pos.z - (m_playerChunkPos.z - m_renderDistance);
        u32 maxBlock = 0;
        const auto *blocks = chunk.blockData();
        for (int i = 0; i < Chunk::VOXELS_PER_CHUNK; ++i) {
            maxBlock = std::max(maxBlock, static_cast<u32>(blocks[i]) & 0xffu);
        }
        core::debugLog(
            "F",
            "world.cpp:uploadChunkVoxelsToGpu",
            "upload_meta",
            "{\"slot\":" + std::to_string(slotIndex) + ",\"lx\":" + std::to_string(localX)
                + ",\"lz\":" + std::to_string(localZ) + ",\"maxBlock\":" + std::to_string(maxBlock)
                + ",\"gridSize\":" + std::to_string(m_gpuChunkGridSize) + "}"
        );
        ++s_uploadLogCount;
    }

    const VkDeviceSize voxelOffsetU32 =
        static_cast<VkDeviceSize>(slotIndex) * static_cast<VkDeviceSize>(Chunk::VOXELS_PER_CHUNK);
    const VkDeviceSize byteOffset = voxelOffsetU32 * sizeof(u32);

    auto *mapped = static_cast<u8 *>(m_gpuVoxelAtlas.map());
    if (!mapped) {
        return;
    }

    auto *dst = reinterpret_cast<u32 *>(mapped + byteOffset);
    const auto *blocks = chunk.blockData();
    const auto *lights = chunk.lightData();

    for (int i = 0; i < Chunk::VOXELS_PER_CHUNK; ++i) {
        const u32 b = static_cast<u32>(blocks[i]) & 0xffu;
        const u32 l = static_cast<u32>(lights[i]) & 0xffu;
        dst[i] = b | (l << 8);
    }

    const VkDeviceSize chunkBytes =
        static_cast<VkDeviceSize>(Chunk::VOXELS_PER_CHUNK) * sizeof(u32);
    m_gpuVoxelAtlas.flushMappedRange(byteOffset, chunkBytes);
    m_gpuVoxelAtlas.unmap();
}

} // namespace wld