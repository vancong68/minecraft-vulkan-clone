#version 450
#extension GL_GOOGLE_include_directive : require

#include "binding.glsl"

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 fragUV;

layout(push_constant) uniform PushConstantsObject {
    uint textureID;
    vec4 color;
    vec4 sun;
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

void main()
{
    vec4 scene = texture(texArr[pco.textureID], fragUV);
    vec3 color = scene.rgb;
    float alpha = scene.a;

    vec2 lightPos = pco.sun.xy;
    float godIntensity = pco.sun.z;

    vec3 godRays = applyGodRays(fragUV, lightPos, godIntensity);
    vec3 finalColor = color + godRays * 0.5;
    finalColor = mix(color, finalColor, clamp(godIntensity, 0.0, 1.0));

    outColor = vec4(finalColor, alpha) * pco.color;
}