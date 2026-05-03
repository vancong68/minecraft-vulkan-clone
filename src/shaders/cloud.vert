#version 450
#extension GL_GOOGLE_include_directive : require

#include "binding.glsl"

vec3 positions[] = vec3[6](
    vec3( 0.0, 0.0,  0.0),
    vec3( 0.0, 0.0,  1.0),
    vec3( 1.0, 0.0,  1.0),

    vec3( 0.0, 0.0,  0.0),
    vec3( 1.0, 0.0,  1.0),
    vec3( 1.0, 0.0,  0.0)
);

layout(location = 1) out vec3 camPos;
layout(location = 2) out vec3 worldPos;

layout(push_constant) uniform PushConstantObject {
    mat4 model;
} pco;

void main()
{
    vec3 pos = positions[gl_VertexIndex] * 12.0;

    camPos = camUbo[CAMERA_UBO_IDX].camPosWs.xyz;
    
    mat4 model = pco.model;
    mat4 view = camUbo[CAMERA_UBO_IDX].view;
    mat4 proj = camUbo[CAMERA_UBO_IDX].proj;

    worldPos = (model * vec4(pos, 1.0)).xyz;
    gl_Position = proj * view * vec4(worldPos, 1.0);
}