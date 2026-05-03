#include "world.hpp"

#include <algorithm>
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

    m_textureID = textureCache.getTextureID("terrain");

    auto binding = ChunkMesh::Vertex::getBindingDescription();
    auto attributes = ChunkMesh::Vertex::getAttributeDescriptions();

    m_pipelines[P_OPAQUE] = gfx::Pipeline::Builder(*m_device)
        .setShader("chunk.vert.spv", VK_SHADER_STAGE_VERTEX_BIT)
        .setShader("chunk.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
        .setVertexInput({
            &binding,
            attributes.data(),
            attributes.size()
        })
        .setPushConstant(sizeof(ChunkPushConstants))
        .setDepthTest(true)
        .setDepthWrite(true)
        .setCull(true)
        .build();

    m_pipelines[P_TRANSPARENT] = gfx::Pipeline::Builder(*m_device)
        .setShader("chunk.vert.spv", VK_SHADER_STAGE_VERTEX_BIT)
        .setShader("chunk.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
        .setVertexInput({
            &binding,
            attributes.data(),
            attributes.size()
        })
        .setPushConstant(sizeof(ChunkPushConstants))
        .setCullMode(VK_CULL_MODE_NONE)
        .setBlending(true)
        .setDepthTest(true)
        .setDepthWrite(true)
        .build();

    m_pipelines[P_CROSS] = gfx::Pipeline::Builder(*m_device)
        .setShader("chunk.vert.spv", VK_SHADER_STAGE_VERTEX_BIT)
        .setShader("chunk.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
        .setVertexInput({
            &binding,
            attributes.data(),
            attributes.size()
        })
        .setPushConstant(sizeof(ChunkPushConstants))
        .setCullMode(VK_CULL_MODE_NONE)
        .setBlending(false)
        .setDepthTest(true)
        .setDepthWrite(true)
        .build();

    // Create shadow map
    m_shadowImage = m_device->createImage(
        2048, 2048,
        VK_FORMAT_D32_SFLOAT,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT
    );
    m_shadowTextureID = m_device->addTexture(m_shadowImage);

    // Create shadow pipeline
    m_shadowPipeline = gfx::Pipeline::Builder(*m_device)
        .setShader("shadow.vert.spv", VK_SHADER_STAGE_VERTEX_BIT)
        .setShader("shadow.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT)
        .setVertexInput({
            &binding,
            attributes.data(),
            attributes.size()
        })
        .setPushConstant(sizeof(ShadowPushConstants))
        .setDepthTest(true)
        .setDepthWrite(true)
        .setCull(true)
        .build();

    m_chunks.reserve(static_cast<size_t>(m_renderDistance) * static_cast<size_t>(m_renderDistance) * 4);
    m_meshes.reserve(static_cast<size_t>(m_renderDistance) * static_cast<size_t>(m_renderDistance) * 4);

    m_generator.init(0);
}

void World::destroy()
{
    for (auto &pipeline : m_pipelines) {
        pipeline.destroy();
    }
    m_shadowPipeline.destroy();

    if (m_shadowTextureID != U32_MAX) {
        m_device->removeResource(m_shadowTextureID);
        m_shadowImage.destroy();
    }

    for (auto &[pos, mesh] : m_meshes) {
        mesh->destroy();
    }

    m_chunks.clear();
    m_meshes.clear();
}

void World::update(const glm::vec3 &playerPos, f32 dt)
{
    ChunkPos newPos = {
        static_cast<i32>(playerPos.x) / Chunk::CHUNK_SIZE,
        static_cast<i32>(playerPos.z) / Chunk::CHUNK_SIZE
    };

    static f32 time = 0.0f;
    time += dt;

    if (time >= 1.0f) {
        time = 0.0f;
        m_updatedChunks = m_pendingChunks.size() + m_pendingMeshes.size();
    }
    
    const int rd = m_renderDistance;
    const f32 squaredDist = static_cast<f32>(rd * rd);

    m_chunksNeeded.clear();
    m_chunksToLoad.clear();
    m_chunksToUnload.clear();

    if (newPos != m_playerChunkPos || m_pendingChunks.empty()) {
        std::queue<ChunkPos> empty;
        std::swap(m_pendingChunks, empty);
        std::swap(m_pendingMeshes, empty);

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
    }

    int chunksLoaded = 0;
    while (!m_pendingChunks.empty() && chunksLoaded < CHUNKS_PER_TICK) {
        ChunkPos pos = m_pendingChunks.front();
        m_pendingChunks.pop();

        if (
            !isChunkLoaded(pos) &&
            m_chunksNeeded.find(pos) != m_chunksNeeded.end()
        ) {
            loadChunks(pos);
            
            m_pendingMeshes.push(pos);
            
            ChunkPos neighbors[4] = {
                {pos.x - 1, pos.z},
                {pos.x + 1, pos.z},
                {pos.x, pos.z - 1},
                {pos.x, pos.z + 1}
            };
            
            for (const auto& neighborPos : neighbors) {
                if (
                    isChunkLoaded(neighborPos)
                    && m_chunksNeeded.find(neighborPos) != m_chunksNeeded.end()
                ) {
                    m_pendingMeshes.push(neighborPos);
                }
            }
            
            chunksLoaded++;
        }
    }

    while (!m_pendingMeshes.empty()) {
        ChunkPos pos = m_pendingMeshes.front();
        m_pendingMeshes.pop();

        if (isChunkLoaded(pos)) {
            updateMeshe(pos);
        }
    }
}

void World::render(
    const core::Camera &camera,
    VkCommandBuffer cmd,
    const glm::mat4 &lightMatrix,
    const glm::vec4 &sunDirPacked,
    u32 shadowMapTextureID,
    bool shadowsEnabled
)
{
    m_frustum = core::Frustum::fromViewProj(
        camera.getView(),
        camera.getProj()
    );

    const u32 shadowTex = shadowsEnabled ? shadowMapTextureID : U32_MAX;

    m_pipelines[P_OPAQUE].bind(cmd);

    for (const auto &[pos, mesh] : m_meshes) {
        f32 x = static_cast<f32>(pos.x * Chunk::CHUNK_SIZE);
        f32 z = static_cast<f32>(pos.z * Chunk::CHUNK_SIZE);

        glm::vec3 min(x, 0.0f, z);
        glm::vec3 max(
            x + Chunk::CHUNK_SIZE,
            Chunk::CHUNK_HEIGHT,
            z + Chunk::CHUNK_SIZE
        );

        if (!m_frustum.isBoxVisible(min, max)) {
            continue;
        }

        ChunkPushConstants pc = {
            .model = glm::translate(glm::mat4(1.0f), {x, 0.0f, z}),
            .shadowMatrix = lightMatrix,
            .sunDir = sunDirPacked,
            .camWorldPos = glm::vec4(camera.getPos(), 0.0f),
            .textureID = m_textureID,
            .shadowMapTextureID = shadowTex,
        };

        m_pipelines[P_OPAQUE].push(cmd, pc);

        mesh->drawOpaque(cmd);
    }

    m_pipelines[P_TRANSPARENT].bind(cmd);

    for (const auto &[pos, mesh] : m_meshes) {
        f32 x = static_cast<f32>(pos.x * Chunk::CHUNK_SIZE);
        f32 z = static_cast<f32>(pos.z * Chunk::CHUNK_SIZE);

        glm::vec3 min(x, 0.0f, z);
        glm::vec3 max(
            x + Chunk::CHUNK_SIZE,
            Chunk::CHUNK_HEIGHT,
            z + Chunk::CHUNK_SIZE
        );

        if (!m_frustum.isBoxVisible(min, max)) {
            continue;
        }

        ChunkPushConstants pc = {
            .model = glm::translate(glm::mat4(1.0f), {x, 0.0f, z}),
            .shadowMatrix = lightMatrix,
            .sunDir = sunDirPacked,
            .camWorldPos = glm::vec4(camera.getPos(), 0.0f),
            .textureID = m_textureID,
            .shadowMapTextureID = shadowTex,
        };

        m_pipelines[P_TRANSPARENT].push(cmd, pc);

        mesh->drawTransparent(cmd);
    }

    m_pipelines[P_CROSS].bind(cmd);

    for (const auto &[pos, mesh] : m_meshes) {
        f32 x = static_cast<f32>(pos.x * Chunk::CHUNK_SIZE);
        f32 z = static_cast<f32>(pos.z * Chunk::CHUNK_SIZE);

        glm::vec3 min(x, 0.0f, z);
        glm::vec3 max(
            x + Chunk::CHUNK_SIZE,
            Chunk::CHUNK_HEIGHT,
            z + Chunk::CHUNK_SIZE
        );

        if (!m_frustum.isBoxVisible(min, max)) {
            continue;
        }

        ChunkPushConstants pc = {
            .model = glm::translate(glm::mat4(1.0f), {x, 0.0f, z}),
            .shadowMatrix = lightMatrix,
            .sunDir = sunDirPacked,
            .camWorldPos = glm::vec4(camera.getPos(), 0.0f),
            .textureID = m_textureID,
            .shadowMapTextureID = shadowTex,
        };

        m_pipelines[P_CROSS].push(cmd, pc);

        mesh->drawCross(cmd);
    }
}

void World::renderShadow(const glm::vec3 &sunDir, VkCommandBuffer cmd)
{
    glm::mat4 lightSpaceMatrix = computeLightMatrix(sunDir);

    const VkImageLayout shadowBefore = m_shadowImage.getLayout();
    m_shadowImage.cmdTransitionLayout(
        cmd,
        shadowBefore,
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        shadowBefore == VK_IMAGE_LAYOUT_UNDEFINED
            ? VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT
            : VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // DEPTH_READ_ONLY after previous frame
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT
    );

    VkRenderingAttachmentInfoKHR depthAttachmentInfo{};
    depthAttachmentInfo.sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR;
    depthAttachmentInfo.imageView = m_shadowImage.getImageView();
    depthAttachmentInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    depthAttachmentInfo.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachmentInfo.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    depthAttachmentInfo.clearValue.depthStencil = {1.0f, 0};

    VkRenderingInfoKHR renderingInfo{};
    renderingInfo.sType = VK_STRUCTURE_TYPE_RENDERING_INFO_KHR;
    renderingInfo.renderArea = {{0, 0}, {2048, 2048}};
    renderingInfo.layerCount = 1;
    renderingInfo.pDepthAttachment = &depthAttachmentInfo;

    vkCmdBeginRendering(cmd, &renderingInfo);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = 2048.0f;
    viewport.height = 2048.0f;
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = {2048, 2048};

    vkCmdSetViewport(cmd, 0, 1, &viewport);
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    m_shadowPipeline.bind(cmd);

    for (const auto &[pos, mesh] : m_meshes) {
        f32 x = static_cast<f32>(pos.x * Chunk::CHUNK_SIZE);
        f32 z = static_cast<f32>(pos.z * Chunk::CHUNK_SIZE);

        ShadowPushConstants pc = {
            .model = glm::translate(glm::mat4(1.0f), {x, 0.0f, z}),
            .shadowMatrix = lightSpaceMatrix,
            .textureID = m_textureID
        };

        m_shadowPipeline.push(cmd, pc);

        mesh->drawOpaque(cmd);
        mesh->drawTransparent(cmd);
        mesh->drawCross(cmd);
    }

    vkCmdEndRendering(cmd);

    m_shadowImage.cmdTransitionLayout(
        cmd,
        VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL,
        VK_IMAGE_LAYOUT_DEPTH_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
    );
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

        updateMeshe(chunkPos);

        if (localPos.x == 0)
            updateMeshe({chunkPos.x - 1, chunkPos.z});
        if (localPos.x == Chunk::CHUNK_SIZE - 1)
            updateMeshe({chunkPos.x + 1, chunkPos.z});
        if (localPos.z == 0)
            updateMeshe({chunkPos.x, chunkPos.z - 1});
        if (localPos.z == Chunk::CHUNK_SIZE - 1)
            updateMeshe({chunkPos.x, chunkPos.z + 1});
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
}

void World::unloadChunks(const ChunkPos &pos)
{
    if (auto it = m_meshes.find(pos); it != m_meshes.end()) {
        it->second->destroy();
        m_meshes.erase(it);
    }

    m_chunks.erase(pos);
}

bool World::isChunkLoaded(const ChunkPos &pos)
{
    return m_chunks.find(pos) != m_chunks.end();
}

void World::updateMeshe(const ChunkPos &pos)
{
    auto chunk = getChunk(pos);

    if (!chunk) { return; }

    std::array<const Chunk *, 4> neighbors = {
        getChunk({pos.x - 1, pos.z}),
        getChunk({pos.x + 1, pos.z}),
        getChunk({pos.x, pos.z - 1}),
        getChunk({pos.x, pos.z + 1})
    };

    UNUSED(neighbors);

    if (auto it = m_meshes.find(pos); it != m_meshes.end()) {
        it->second->update(*chunk, neighbors);
    } else {
        auto mesh = std::make_unique<ChunkMesh>();
        mesh->init(*m_device);
        mesh->generate(*chunk, neighbors);
        m_meshes[pos] = std::move(mesh);
    }
}

} // namespace wld