#version 450
#extension GL_GOOGLE_include_directive : require

#include "binding.glsl"

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 fragUV;

layout(push_constant) uniform PushConstantsObject {
    uint textureID;
    uint depthTextureID;
    uint shadowTextureID;
    uint _padding;
    vec4 color;
    vec4 sun;
    uint effects;
} pco;

vec3 applyGodRays(vec2 uv, vec2 lightPos, float intensity)
{
    vec2 delta = lightPos - uv;
    float dist = length(delta);
    vec2 step = delta * 0.02;

    vec3 illumination = vec3(0.0);
    float decay = 0.95;
    float weight = 0.8;
    vec2 sampleUV = uv;
    float illuminationDecay = 1.0;

    for (int i = 0; i < 24; ++i) {
        sampleUV += step;
        vec3 sampleColor = texture(texArr[pco.textureID], sampleUV).rgb;
        float lum = max(dot(sampleColor, vec3(0.299, 0.587, 0.114)), 0.0);
        illumination += sampleColor * lum * illuminationDecay * weight;
        illuminationDecay *= decay;
    }

    float attenuation = smoothstep(0.0, 0.8, 1.0 - dist);
    return illumination * intensity * attenuation;
}

float applySSAO(vec2 uv)
{
    float depth = texture(texArr[pco.depthTextureID], uv).r;

    if (depth >= 1.0) {
        return 1.0;
    }

    float occlusion = 0.0;
    const int ssaoSampleCount = 16;

    vec2 ssaoOffsets[ssaoSampleCount] = vec2[](
        vec2(-0.5, -0.5), vec2(0.5, -0.5), vec2(-0.5, 0.5), vec2(0.5, 0.5),
        vec2(0.0, -0.7), vec2(0.0, 0.7), vec2(-0.7, 0.0), vec2(0.7, 0.0),
        vec2(-0.4, -0.4), vec2(0.4, -0.4), vec2(-0.4, 0.4), vec2(0.4, 0.4),
        vec2(-0.3, -0.6), vec2(0.3, -0.6), vec2(-0.6, 0.3), vec2(0.6, 0.3)
    );

    float radius = 0.02;
    float bias = 0.025;

    for (int i = 0; i < ssaoSampleCount; ++i) {
        vec2 sampleUV = uv + ssaoOffsets[i] * radius;
        float sampleDepth = texture(texArr[pco.depthTextureID], sampleUV).r;

        float rangeCheck = smoothstep(0.0, 1.0, radius / abs(depth - sampleDepth));
        occlusion += (sampleDepth >= depth + bias ? 1.0 : 0.0) * rangeCheck;
    }

    occlusion = 1.0 - (occlusion / float(ssaoSampleCount));
    return clamp(occlusion, 0.0, 1.0);
}

void main()
{
    vec4 scene = texture(texArr[pco.textureID], fragUV);
    vec3 color = scene.rgb;
    float alpha = scene.a;

    vec2 lightPos = pco.sun.xy;
    float godIntensity = pco.sun.z;

    vec3 godRays = vec3(0.0);
    if ((pco.effects & 2u) != 0u) {
        godRays = applyGodRays(fragUV, lightPos, godIntensity);
    }

    vec3 finalColor = color + godRays * 0.5;
    finalColor = mix(color, finalColor, clamp(godIntensity, 0.0, 1.0));

    float ssao = 1.0;
    if ((pco.effects & 1u) != 0u) {
        ssao = applySSAO(fragUV);
    }

    finalColor *= ssao * 0.8 + 0.2;

    outColor = vec4(finalColor, alpha) * pco.color;
}
