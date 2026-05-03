#version 450

layout(location = 0) in vec3 inPos;
layout(location = 1) in vec2 inUV;
layout(location = 2) in uint inLightLevel;
layout(location = 3) in uint inFaceDirection;

layout(push_constant) uniform PushConstantObject {
    mat4 model;
    mat4 shadowMatrix;
    uint textureId;
} pco;

void main()
{
    vec4 worldPos = pco.model * vec4(inPos, 1.0);
    gl_Position = pco.shadowMatrix * worldPos;
}
