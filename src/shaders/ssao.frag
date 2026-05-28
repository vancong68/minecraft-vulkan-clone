#version 450
#extension GL_GOOGLE_include_directive : require

#include "binding.glsl"

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec2 fragUV;

layout(push_constant) uniform PushConstantsObject {
    uint depthTextureId;
    uint _pad0;
    uint _pad1;
    uint _pad2;
} pco;

float ssao(vec2 uv)
{
    float depth = texture(texArr[pco.depthTextureId], uv).r;
    if (depth >= 1.0) return 1.0;

    float occlusion = 0.0;
    const int n = 16;

    vec2 offs[n] = vec2[](
        vec2(-0.5, -0.5), vec2(0.5, -0.5), vec2(-0.5, 0.5), vec2(0.5, 0.5),
        vec2(0.0, -0.7), vec2(0.0, 0.7), vec2(-0.7, 0.0), vec2(0.7, 0.0),
        vec2(-0.4, -0.4), vec2(0.4, -0.4), vec2(-0.4, 0.4), vec2(0.4, 0.4),
        vec2(-0.3, -0.6), vec2(0.3, -0.6), vec2(-0.6, 0.3), vec2(0.6, 0.3)
    );

    float radius = 0.02;
    float bias = 0.02;
    for (int i = 0; i < n; ++i) {
        vec2 suv = uv + offs[i] * radius;
        float sd = texture(texArr[pco.depthTextureId], suv).r;
        float rangeCheck = smoothstep(0.0, 1.0, radius / max(abs(depth - sd), 1e-4));
        occlusion += (sd >= depth + bias ? 1.0 : 0.0) * rangeCheck;
    }
    occlusion = 1.0 - (occlusion / float(n));
    return clamp(occlusion, 0.0, 1.0);
}

void main()
{
    float ao = ssao(fragUV);
    outColor = vec4(ao, ao, ao, 1.0);
}

