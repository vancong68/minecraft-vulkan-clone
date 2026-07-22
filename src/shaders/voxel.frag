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
    uint aoTextureId;
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
    uint gridId = nonuniformEXT(pco.chunkGridSsboId);
    uint originX_u = ssboArr[nonuniformEXT(gridId)].data[0];
    uint originZ_u = ssboArr[nonuniformEXT(gridId)].data[1];
    uint gridSize = ssboArr[nonuniformEXT(gridId)].data[2];

    int originX = int(originX_u);
    int originZ = int(originZ_u);

    int relX = chunkX - originX;
    int relZ = chunkZ - originZ;
    if (relX < 0 || relZ < 0) return INVALID_SLOT;
    if (relX >= int(gridSize) || relZ >= int(gridSize)) return INVALID_SLOT;

    uint idx = uint(relZ) * gridSize + uint(relX);
    return ssboArr[nonuniformEXT(gridId)].data[4u + idx];
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

    uint atlasId = nonuniformEXT(pco.voxelAtlasSsboId);
    uint base = slot * uint(VOXELS_PER_CHUNK);
    return ssboArr[nonuniformEXT(atlasId)].data[base + uint(localIndex)];
}

uint sampleBlockId(ivec3 worldVoxel)
{
    return sampleVoxelPacked(worldVoxel) & 0xffu;
}

vec2 getFaceUV(uint blockId, uint faceId, vec2 faceFrac)
{
    uint uvId = nonuniformEXT(pco.blockUvSsboId);
    uint uvPacked = ssboArr[nonuniformEXT(uvId)].data[blockId * 6u + faceId];
    uint tileX = uvPacked & 0xffffu;
    uint tileY = (uvPacked >> 16u) & 0xffffu;

    const float tilesPerAxis = 16.0;
    const float atlasPixels = 256.0;
    float flippedTileY = tilesPerAxis - 1.0 - float(tileY);
    vec2 tile = vec2(float(tileX), flippedTileY);

    vec2 halfTexel = vec2(0.5 / atlasPixels);
    vec2 tileMin = tile / tilesPerAxis + halfTexel;
    vec2 tileMax = (tile + vec2(1.0)) / tilesPerAxis - halfTexel;
    vec2 clampedFrac = clamp(faceFrac, vec2(0.0), vec2(1.0));

    return mix(tileMin, tileMax, clampedFrac);
}

vec3 computeLighting(vec3 albedo, vec3 normal, vec3 sunDir, float ao)
{
    float sunNdotL = max(dot(normal, -sunDir), 0.0);
    float sunIntensity = 0.82;
    float ambientLight = 0.18;
    
    vec3 sunLight = albedo * sunIntensity * sunNdotL;
    vec3 ambientComponent = albedo * ambientLight;
    
    vec3 lit = (sunLight + ambientComponent) * ao;
    
    return lit;
}

void main()
{
    vec2 ndc = fragUV * 2.0 - 1.0;
    vec4 clip = vec4(ndc, 1.0, 1.0);
    mat4 invProj = inverse(camUbo[CAMERA_UBO_IDX].proj);
    mat4 invView = inverse(camUbo[CAMERA_UBO_IDX].view);

    vec4 viewPos = invProj * clip;
    viewPos /= max(viewPos.w, 1e-6);
    vec3 dirWs = normalize((invView * vec4(viewPos.xyz, 0.0)).xyz);
    vec3 originWs = camUbo[CAMERA_UBO_IDX].camPosWs.xyz;

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
            break;
        }
    }

    if (hitBlock == 0u) {
        float tSky = clamp(rayDir.y * 0.5 + 0.5, 0.0, 1.0);
        vec3 sky = mix(vec3(0.65, 0.75, 0.95), vec3(0.2, 0.35, 0.7), tSky);
        outColor = vec4(sky, 1.0);
        gl_FragDepth = 1.0;
        return;
    }

    vec3 hitPos = rayOrig + rayDir * t;
    vec4 clipHit = camUbo[CAMERA_UBO_IDX].proj * camUbo[CAMERA_UBO_IDX].view * vec4(hitPos, 1.0);
    float ndcZ = clipHit.z / max(clipHit.w, 1e-6);
    gl_FragDepth = clamp(ndcZ, 0.0, 1.0);

    vec3 local = fract(hitPos);
    uint faceId = 0u;
    vec2 faceFrac = vec2(0.0);

    if (hitNormal.x == 1) { faceId = 3u; faceFrac = vec2(1.0 - local.z, local.y); }
    else if (hitNormal.x == -1) { faceId = 2u; faceFrac = vec2(local.z, local.y); }
    else if (hitNormal.z == 1) { faceId = 1u; faceFrac = vec2(local.x, local.y); }
    else if (hitNormal.z == -1) { faceId = 0u; faceFrac = vec2(1.0 - local.x, local.y); }
    else if (hitNormal.y == 1) { faceId = 5u; faceFrac = vec2(local.x, 1.0 - local.z); }
    else { faceId = 4u; faceFrac = vec2(local.x, local.z); }

    vec2 uv = getFaceUV(hitBlock, faceId, faceFrac);
    vec3 albedo = texture(texArr[nonuniformEXT(pco.terrainTextureId)], uv).rgb;

    float ao = 1.0;
    if (pco.aoTextureId != 0xffffffffu) {
        ao = texture(texArr[nonuniformEXT(pco.aoTextureId)], fragUV).r;
    } else {
        // Simple voxel-based AO: sample 6 axis neighbors and reduce light based on occupancy.
        int occ = 0;
        ivec3 offs[6] = ivec3[](ivec3(1,0,0), ivec3(-1,0,0), ivec3(0,1,0), ivec3(0,-1,0), ivec3(0,0,1), ivec3(0,0,-1));
        for (int i = 0; i < 6; ++i) {
            uint sid = sampleBlockId(hitVoxel + offs[i]);
            if (sid != 0u) ++occ;
        }
        ao = clamp(1.0 - float(occ) / 6.0 * 0.6, 0.0, 1.0);
    }

    vec3 n = normalize(vec3(hitNormal));
    vec3 sunDir = normalize(pco.sunDir_ws.xyz);
    
    vec3 lit = computeLighting(albedo, n, sunDir, ao);

    outColor = vec4(lit, 1.0);
}

