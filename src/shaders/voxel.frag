#version 450
#extension GL_GOOGLE_include_directive : require

#include "binding.glsl"

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec2 fragUV;

layout(push_constant) uniform PushConstantsObject {
    uint chunkGridSsboId;
    uint voxelAtlasSsboId;
    uint blockUvSsboId;
    uint terrainTextureId;
    vec4 sunDir_ws;
} pco;

const int CHUNK_SIZE = 16;
const int CHUNK_HEIGHT = 128;
const int VOXELS_PER_CHUNK = CHUNK_SIZE * CHUNK_HEIGHT * CHUNK_SIZE;
const uint INVALID_SLOT = 0xffffffffu;

int floorDiv(int a, int b)
{
    int q = a / b;
    int r = a - q * b;
    if ((r != 0) && ((r < 0) != (b < 0))) {
        q -= 1;
    }
    return q;
}

int posMod(int a, int b)
{
    int m = a % b;
    if (m < 0) m += b;
    return m;
}

uint getChunkSlot(int chunkX, int chunkZ)
{
    uint gridId = pco.chunkGridSsboId;
    uint originX_u = ssboArr[gridId].data[0];
    uint originZ_u = ssboArr[gridId].data[1];
    uint gridSize = ssboArr[gridId].data[2];

    int originX = int(originX_u);
    int originZ = int(originZ_u);

    int relX = chunkX - originX;
    int relZ = chunkZ - originZ;
    if (relX < 0 || relZ < 0) return INVALID_SLOT;
    if (relX >= int(gridSize) || relZ >= int(gridSize)) return INVALID_SLOT;

    uint idx = uint(relZ) * gridSize + uint(relX);
    return ssboArr[gridId].data[4u + idx];
}

uint sampleVoxelPacked(ivec3 worldVoxel)
{
    if (worldVoxel.y < 0 || worldVoxel.y >= CHUNK_HEIGHT) {
        return 0u;
    }

    int chunkX = floorDiv(worldVoxel.x, CHUNK_SIZE);
    int chunkZ = floorDiv(worldVoxel.z, CHUNK_SIZE);

    uint slot = getChunkSlot(chunkX, chunkZ);
    if (slot == INVALID_SLOT) {
        return 0u;
    }

    int localX = posMod(worldVoxel.x, CHUNK_SIZE);
    int localZ = posMod(worldVoxel.z, CHUNK_SIZE);
    int localIndex = worldVoxel.y * (CHUNK_SIZE * CHUNK_SIZE) + localZ * CHUNK_SIZE + localX;

    uint atlasId = pco.voxelAtlasSsboId;
    uint base = slot * uint(VOXELS_PER_CHUNK);
    return ssboArr[atlasId].data[base + uint(localIndex)];
}

uint sampleBlockId(ivec3 worldVoxel)
{
    return sampleVoxelPacked(worldVoxel) & 0xffu;
}

vec2 getFaceUV(uint blockId, uint faceId, vec2 faceFrac)
{
    // Face UV table: packed (x | (y<<16)) per [blockId*6 + faceId].
    uint uvPacked = ssboArr[pco.blockUvSsboId].data[blockId * 6u + faceId];
    uint tileX = uvPacked & 0xffffu;
    uint tileY = (uvPacked >> 16u) & 0xffffu;

    // Atlas is 256px with 16px tiles => 16x16 tiles.
    const float tilesPerAxis = 16.0;
    vec2 tile = vec2(float(tileX), float(tileY));
    return (tile + faceFrac) / tilesPerAxis;
}

void main()
{
    // Ray reconstruction
    vec2 ndc = fragUV * 2.0 - 1.0;
    vec4 clip = vec4(ndc, 1.0, 1.0);
    mat4 invProj = inverse(camUbo[CAMERA_UBO_IDX].proj);
    mat4 invView = inverse(camUbo[CAMERA_UBO_IDX].view);

    vec4 viewPos = invProj * clip;
    viewPos /= max(viewPos.w, 1e-6);
    vec3 dirWs = normalize((invView * vec4(viewPos.xyz, 0.0)).xyz);
    vec3 originWs = camUbo[CAMERA_UBO_IDX].camPosWs.xyz;

    // DDA setup
    vec3 rayDir = dirWs;
    vec3 rayOrig = originWs;

    ivec3 voxel = ivec3(floor(rayOrig));

    ivec3 stepI = ivec3(sign(rayDir));

    vec3 safeDir = mix(rayDir, vec3(1e-6), lessThan(abs(rayDir), vec3(1e-6)));
    vec3 tDelta = abs(1.0 / safeDir);

    vec3 voxelBorder = vec3(voxel) + step(vec3(0.0), rayDir);
    vec3 tMax = (voxelBorder - rayOrig) / safeDir;
    tMax = max(tMax, vec3(0.0));

    float t = 0.0;
    float maxT = 256.0;

    uint hitBlock = 0u;
    ivec3 hitVoxel = voxel;
    ivec3 hitNormal = ivec3(0);

    for (int i = 0; i < 128; ++i) {
        if (t > maxT) break;

        uint b = sampleBlockId(voxel);
        if (b != 0u) {
            hitBlock = b;
            hitVoxel = voxel;
            break;
        }

        // advance
        if (tMax.x < tMax.y) {
            if (tMax.x < tMax.z) {
                voxel.x += stepI.x;
                t = tMax.x;
                tMax.x += tDelta.x;
                hitNormal = ivec3(-stepI.x, 0, 0);
            } else {
                voxel.z += stepI.z;
                t = tMax.z;
                tMax.z += tDelta.z;
                hitNormal = ivec3(0, 0, -stepI.z);
            }
        } else {
            if (tMax.y < tMax.z) {
                voxel.y += stepI.y;
                t = tMax.y;
                tMax.y += tDelta.y;
                hitNormal = ivec3(0, -stepI.y, 0);
            } else {
                voxel.z += stepI.z;
                t = tMax.z;
                tMax.z += tDelta.z;
                hitNormal = ivec3(0, 0, -stepI.z);
            }
        }

        if (voxel.y < 0 || voxel.y >= CHUNK_HEIGHT) {
            // Give up when leaving world vertically.
            break;
        }
    }

    if (hitBlock == 0u) {
        // Simple sky gradient
        float tSky = clamp(rayDir.y * 0.5 + 0.5, 0.0, 1.0);
        vec3 sky = mix(vec3(0.65, 0.75, 0.95), vec3(0.2, 0.35, 0.7), tSky);
        outColor = vec4(sky, 1.0);
        gl_FragDepth = 1.0;
        return;
    }

    // Hit point and depth
    vec3 hitPos = rayOrig + rayDir * t;
    vec4 clipHit = camUbo[CAMERA_UBO_IDX].proj * camUbo[CAMERA_UBO_IDX].view * vec4(hitPos, 1.0);
    float ndcZ = clipHit.z / max(clipHit.w, 1e-6);
    gl_FragDepth = clamp(ndcZ, 0.0, 1.0);

    // Compute face UVs
    vec3 local = fract(hitPos);
    uint faceId = 0u;
    vec2 faceFrac = vec2(0.0);

    if (hitNormal.x == 1) { faceId = 3u; faceFrac = vec2(1.0 - local.z, local.y); }      // WEST
    else if (hitNormal.x == -1) { faceId = 2u; faceFrac = vec2(local.z, local.y); }      // EAST
    else if (hitNormal.z == 1) { faceId = 1u; faceFrac = vec2(local.x, local.y); }       // SOUTH
    else if (hitNormal.z == -1) { faceId = 0u; faceFrac = vec2(1.0 - local.x, local.y); }// NORTH
    else if (hitNormal.y == 1) { faceId = 5u; faceFrac = vec2(local.x, 1.0 - local.z); } // BOTTOM
    else { faceId = 4u; faceFrac = vec2(local.x, local.z); }                             // TOP

    vec2 uv = getFaceUV(hitBlock, faceId, faceFrac);
    vec3 albedo = texture(texArr[pco.terrainTextureId], uv).rgb;

    vec3 n = normalize(vec3(hitNormal));
    vec3 sunDir = normalize(pco.sunDir_ws.xyz);
    float ndotl = max(dot(n, -sunDir), 0.0);
    vec3 lit = albedo * (0.18 + 0.82 * ndotl);

    outColor = vec4(lit, 1.0);
}

