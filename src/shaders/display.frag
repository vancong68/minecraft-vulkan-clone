#version 450
#extension GL_GOOGLE_include_directive : require

#include "binding.glsl"

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 fragUV;

layout(push_constant) uniform PushConstantsObject {
    uint textureID;
    uint depthTextureID;
    uint aoTextureID;
    uint godRaysTextureID;
    uint shadowTextureID;
    uint _padding0;
    vec4 color;
    vec4 sun;
    uint effects;
} pco;

void main()
{
    vec4 scene = texture(texArr[pco.textureID], fragUV);
    vec3 color = scene.rgb;
    float alpha = scene.a;

    vec2 lightPos = pco.sun.xy;
    float godIntensity = pco.sun.z;

    vec3 godRays = vec3(0.0);
    if ((pco.effects & 2u) != 0u) {
        if (pco.godRaysTextureID != 0xffffffffu) {
            godRays = texture(texArr[pco.godRaysTextureID], fragUV).rgb * godIntensity;
        }
    }

    vec3 finalColor = color + godRays * 0.5;
    finalColor = mix(color, finalColor, clamp(godIntensity, 0.0, 1.0));

    float ssao = 1.0;
    if ((pco.effects & 1u) != 0u) {
        if (pco.aoTextureID != 0xffffffffu) {
            ssao = texture(texArr[pco.aoTextureID], fragUV).r;
        }
    }

    finalColor *= ssao * 0.8 + 0.2;

    outColor = vec4(finalColor, alpha) * pco.color;
}
