#version 450
#extension GL_GOOGLE_include_directive : require

#include "binding.glsl"

layout(location = 0) out vec4 outColor;

layout(location = 0) in vec2 fragUV;
layout(location = 1) in vec3 camPos;
layout(location = 2) in vec3 worldPos;
layout(location = 3) in flat uint lightLevel;
layout(location = 4) in flat uint faceDirection;

layout(push_constant) uniform PushConstantObject {
    mat4 model;
    uint textureId;
} pco;

vec3 addShadow(vec3 color)
{
    float faceLights[6] = float[](
        0.55, 0.55, 0.8, 0.8, 1.0, 0.35
    );

    vec3 faceNormals[6] = vec3[](
        vec3(0.0, 0.0, -1.0),
        vec3(0.0, 0.0, 1.0),
        vec3(1.0, 0.0, 0.0),
        vec3(-1.0, 0.0, 0.0),
        vec3(0.0, 1.0, 0.0),
        vec3(0.0, -1.0, 0.0)
    );

    vec3 sunDir = normalize(vec3(-0.4, -1.0, -0.2));
    float ndotl = max(dot(faceNormals[faceDirection], -sunDir), 0.0);
    float brightness = pow(lightLevel / 15.0, 2.2);
    float light = brightness * faceLights[faceDirection];
    float sunShade = mix(0.4, 1.0, ndotl);

    float ambientLight = 0.08;
    vec3 litColor = mix(color * ambientLight, color, light * sunShade);

    float heightShadow = clamp((worldPos.y - 12.0) / 24.0, 0.0, 1.0);
    litColor *= mix(0.75, 1.0, heightShadow);

    return litColor;
}

vec3 addFog(vec3 color, float dist)
{
    float fogStart = 4.0 * 16.0;
    float fogEnd = 7.0 * 16.0;
    vec3 fogColor = vec3(0.73, 0.83, 1.0);

    float fogFactor = 1.0 - clamp(
        (fogEnd - dist) / (fogEnd - fogStart),
        0.0,
        1.0
    );

    fogFactor = pow(fogFactor, 0.7);

    return mix(color, fogColor, fogFactor);
}

void main()
{
    float dist = length(worldPos - camPos);

    vec3 color = texture(texArr[pco.textureId], fragUV).rgb;
    float alpha = texture(texArr[pco.textureId], fragUV).a;

    if (alpha > 0.1 && alpha < 0.9) {
        alpha = 0.8;
    }

    if (alpha < 0.1) {
        discard;
    }

    color = addShadow(color);

    color = addFog(color, dist);


    outColor = vec4(color, alpha);
}