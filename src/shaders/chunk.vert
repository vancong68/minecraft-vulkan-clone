#version 450
#extension GL_GOOGLE_include_directive : require

#include "binding.glsl"

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in uint inLightLevel;
layout(location = 3) in uint inFaceDirection;

layout(location = 0) out vec2 fragUV;
layout(location = 1) out vec3 worldPos;
layout(location = 2) out flat uint lightLevel;
layout(location = 3) out flat uint faceDirection;

layout(push_constant) uniform PushConstantObject {
    layout(offset = 0) mat4 model;
    layout(offset = 64) mat4 shadowMatrix;
    layout(offset = 128) vec4 sunDir;
    layout(offset = 144) vec4 camWorldPos;
    layout(offset = 160) uint textureId;
    layout(offset = 164) uint shadowMapId;
} pco;

void main()
{
    fragUV = inUV;
    lightLevel = inLightLevel;
    faceDirection = inFaceDirection;

    mat4 view = camUbo[CAMERA_UBO_IDX].view;
    mat4 proj = camUbo[CAMERA_UBO_IDX].proj;

    worldPos = (pco.model * vec4(inPos, 1.0)).xyz;
    gl_Position = proj * view * vec4(worldPos, 1.0);
}
