#version 450
#extension GL_GOOGLE_include_directive : require

#include "binding.glsl"

layout(location = 0) out vec4 outColor;
layout(location = 0) in vec2 fragUV;

layout(push_constant) uniform PushConstantsObject {
    uint maskTextureId;
    uint _pad0;
    vec2 sunScreen01;
    float intensity;
    uint _pad1;
} pco;

void main()
{
    vec2 lightPos = pco.sunScreen01;
    vec2 delta = lightPos - fragUV;
    float dist = length(delta);

    vec2 stepUv = delta * (1.0 / 32.0);
    vec2 sampleUv = fragUV;

    float illum = 0.0;
    float decay = 0.95;
    float weight = 1.0;
    float illumDecay = 1.0;

    for (int i = 0; i < 32; ++i) {
        sampleUv += stepUv;
        float m = texture(texArr[pco.maskTextureId], sampleUv).r;
        illum += m * illumDecay * weight;
        illumDecay *= decay;
    }

    float attenuation = smoothstep(0.0, 0.85, 1.0 - dist);
    float rays = illum * pco.intensity * attenuation * (1.0 / 32.0);

    outColor = vec4(vec3(rays), 1.0);
}

